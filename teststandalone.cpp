//g++ -Wno-narrowing -o teststandalone teststandalone.cpp sbc.c libsbc/src/bits.c libsbc/src/sbc.c -lasound

#include <iostream>
#include <fstream>
#include <alsa/asoundlib.h>
#include "libsbc/include/sbc.h"


#define SAMPLE_RATE 44100
#define CHANNELS 2
#define BUFFER_SIZE 4096


int main(int argc, char *argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <sbc-audio-file>\n";
        return 1;
    }

    FILE* fp_in = fopen(argv[1], "rb");
    if (!fp_in) {
        std::cerr << "Error opening file: " << argv[1] << "\n";
        return 1;
    }

    /* --- Setup decoding --- */

    static const char *sbc_mode_str[] = {
        [SBC_MODE_MONO        ] = "Mono",
        [SBC_MODE_DUAL_CHANNEL] = "Dual-Channel",
        [SBC_MODE_STEREO      ] = "Stereo",
        [SBC_MODE_JOINT_STEREO] = "Joint-Stereo"
    };



    uint8_t data[2*SBC_MAX_SAMPLES*sizeof(int16_t)] = {0};
    int16_t pcml[SBC_MAX_SAMPLES] = {0};
    int16_t pcmr[SBC_MAX_SAMPLES] = {0};
    struct sbc_frame frame = {0};
    sbc_t sbc = {0};

    if (fread(data, SBC_PROBE_SIZE, 1, fp_in) < 1
            || sbc_probe(data, &frame) < 0) {
        std::cerr << "Error probing SBC frame\n";
    }

    int srate_hz = sbc_get_freq_hz(frame.freq);

    fprintf(stderr, "%s %d Hz -- %.1f kbps (bitpool %d)"
                            " -- %d blocks, %d subbands\n",
        sbc_mode_str[frame.mode], srate_hz,
        sbc_get_frame_bitrate(&frame) * 1e-3, frame.bitpool,
        frame.nblocks, frame.nsubbands);

    int nch = 1 + (frame.mode != SBC_MODE_MONO);

    unsigned int sample_rate = sbc_get_freq_hz(frame.freq);
    int dir;

    snd_pcm_t* handle;
    snd_pcm_hw_params_t* params;

    if (snd_pcm_open(&handle, "default", SND_PCM_STREAM_PLAYBACK, 0) < 0) {
        std::cerr << "Error opening ALSA playback device\n";
        return 1;
    }

    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_NONINTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle, params, CHANNELS);
    snd_pcm_hw_params_set_rate_near(handle, params, &sample_rate, &dir);
    snd_pcm_hw_params(handle, params);


    sbc_reset(&sbc);

    /* --- Decoding loop --- */

    for (int i = 0; i == 0 || (fread(data, SBC_PROBE_SIZE, 1, fp_in) >= 1
                               && sbc_probe(data, &frame) == 0); i++) {

        std::cout << sbc_get_frame_size(&frame) + SBC_PROBE_SIZE << " bytes in frame " << i << "\n";
        if (fread(data + SBC_PROBE_SIZE,
                sbc_get_frame_size(&frame) - SBC_PROBE_SIZE, 1, fp_in) < 1) {
                printf("break");
            break;
        }

        sbc_decode(&sbc, data, sizeof(data),
            &frame, pcml, pcmr);

        int npcm = frame.nblocks * frame.nsubbands;

        //wave_write_pcm(fp_out, sizeof(*pcm), pcm, nch, 0, npcm);
        snd_pcm_writen(handle, (void*[]){pcml, pcmr}, npcm);

    }


    snd_pcm_drain(handle);
    snd_pcm_close(handle);


}
