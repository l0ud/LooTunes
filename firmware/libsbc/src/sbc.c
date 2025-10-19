/******************************************************************************
 *
 *  Copyright 2022 Google LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at:
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ******************************************************************************/

#include "../include/sbc.h"
#include <string.h>
#include "stdio.h"

#undef  SBC_BITS_TRACE
#include "bits.h"


/**
 * SBC Frame header size
 */

#define SBC_HEADER_SIZE   ( 4)


/**
 * Assembly declaration
 */

void sbc_synthesize_4(struct sbc_dstate *state,
    const int16_t *in, int scale, int16_t *out);

void sbc_synthesize_8(struct sbc_dstate *state,
    const int16_t *in, int scale, int16_t *out);

#ifndef SBC_ASM
#define ASM(fn) (fn##_c)
#else
#define ASM(fn) (fn)
#endif


/**
 * Macros
 * MIN/MAX  Minimum and maximum between 2 values
 * SAT16    Signed saturation on 16 bits
 * Saturate on 16 bits
 */

#define SBC_MIN(a, b)  ( (a) < (b) ?  (a) : (b) )
#define SBC_MAX(a, b)  ( (a) > (b) ?  (a) : (b) )

#define SBC_SAT16(v) (int16_t)\
    ( (v) > INT16_MAX ? INT16_MAX : \
      (v) < INT16_MIN ? INT16_MIN : (v) )


/**
 * Count number of leading zero bits
 */

#define SBC_CLZ(n) __builtin_clz(n)
/**
 * mSBC constant frame description
 */
static const struct sbc_frame msbc_frame = {
    .msbc = true,
    .freq = SBC_FREQ_16K,
    .mode = SBC_MODE_MONO,
    .bam = SBC_BAM_LOUDNESS,
    .nblocks = 15, .nsubbands = 8,
    .bitpool = 26
};


/* ----------------------------------------------------------------------------
 *  Common
 * ------------------------------------------------------------------------- */

/**
 * Check a frame description
 * frame           The frame description to check
 * return          True when valid, False otherwise
 */
static bool check_frame(const struct sbc_frame *frame)
{
    /* --- Check number of sub-blocks and sub-bands --- */

    if ((unsigned)(frame->nblocks - 4) > 12 ||
            (!frame->msbc && frame->nblocks % 4 != 0))
        return false;

    if ((unsigned)(frame->nsubbands - 4) > 4 || frame->nsubbands % 4 != 0)
        return false;

    /* --- Validate the bitpool value --- */

    bool two_channels = (frame->mode != SBC_MODE_MONO);
    bool dual_mode = (frame->mode == SBC_MODE_DUAL_CHANNEL);
    bool joint_mode = (frame->mode == SBC_MODE_JOINT_STEREO);
    bool stereo_mode = joint_mode || (frame->mode == SBC_MODE_STEREO);

    int max_bits =
        ((16 * frame->nsubbands * frame->nblocks) << two_channels) -
        (SBC_HEADER_SIZE * 8) -
        ((4 * frame->nsubbands) << two_channels) -
        (joint_mode ? frame->nsubbands : 0);

    int max_bitpool = SBC_MIN( max_bits / (frame->nblocks << dual_mode),
                               (16 << stereo_mode) * frame->nsubbands );

    return frame->bitpool <= max_bitpool;
}

/**
 * Compute the bit distribution for Independent or Stereo Channel
 * frame           Frame description
 * scale_factors   Scale-factor values
 * nbits           Return of allocated bits for each channels / subbands
 */
static void compute_nbits(const struct sbc_frame *frame,
    const int (*scale_factors)[SBC_MAX_SUBBANDS],
    int (*nbits)[SBC_MAX_SUBBANDS])
{
    /* --- Offsets of "Loudness" bit allocation --- */

    static const int loudness_offset_4[SBC_NUM_FREQ][4] = {
        [SBC_FREQ_16K ] = { -1,  0,  0,  0 },
        [SBC_FREQ_32K ] = { -2,  0,  0,  1 },
        [SBC_FREQ_44K1] = { -2,  0,  0,  1 },
        [SBC_FREQ_48K ] = { -2,  0,  0,  1 },
    };

    static const int loudness_offset_8[SBC_NUM_FREQ][8] = {
        [SBC_FREQ_16K ] = { -2,  0,  0,  0,  0,  0,  0,  1 },
        [SBC_FREQ_32K ] = { -3,  0,  0,  0,  0,  0,  1,  2 },
        [SBC_FREQ_44K1] = { -4,  0,  0,  0,  0,  0,  1,  2 },
        [SBC_FREQ_48K ] = { -4,  0,  0,  0,  0,  0,  1,  2 },
    };

    /* --- Compute the number of bits needed --- */

    const int *loudness_offset = frame->nsubbands == 4 ?
        loudness_offset_4[frame->freq] : loudness_offset_8[frame->freq];

    bool stereo_mode = frame->mode == SBC_MODE_STEREO ||
                       frame->mode == SBC_MODE_JOINT_STEREO;

    int nsubbands = frame->nsubbands;
    int nchannels = 1 + stereo_mode;

    int bitneeds[2][SBC_MAX_SUBBANDS];
    int max_bitneed = 0;

    for (int ich = 0; ich < nchannels; ich++)
        for (int isb = 0; isb < nsubbands; isb++) {
            int bitneed, scf = scale_factors[ich][isb];

            if (frame->bam == SBC_BAM_LOUDNESS) {
                bitneed = scf ? scf - loudness_offset[isb] : -5;
                bitneed >>= (bitneed > 0);
            } else {
                bitneed = scf;
            }

            if (bitneed > max_bitneed)
                max_bitneed = bitneed;

            bitneeds[ich][isb] = bitneed;
        }

    /* --- Loop over the bit distribution, until reaching the bitpool --- */

    int bitpool = frame->bitpool;

    int bitcount = 0;
    int bitslice = max_bitneed + 1;

    for (int bc = 0; bc < bitpool; ) {

        int bs = bitslice--;
        bitcount = bc;
        if (bitcount == bitpool)
            break;

        for (int ich = 0; ich < nchannels; ich++)
            for (int isb = 0; isb < nsubbands; isb++) {
                int bn = bitneeds[ich][isb];
                bc += (bn >= bs && bn < bs + 15) + (bn == bs);
            }
    }

    /* --- Bits distribution --- */

    for (int ich = 0; ich < nchannels; ich++)
        for (int isb = 0; isb < nsubbands; isb++) {
            int nbit = bitneeds[ich][isb] - bitslice;
            nbits[ich][isb] = nbit < 2 ? 0 : nbit > 16 ? 16 : nbit;
        }

    /* --- Allocate remaining bits --- */

    for (int isb = 0; isb < nsubbands && bitcount < bitpool; isb++)
        for (int ich = 0; ich < nchannels && bitcount < bitpool; ich++) {

            int n = nbits[ich][isb] && nbits[ich][isb] < 16 ? 1 :
                    bitneeds[ich][isb] == bitslice + 1 &&
                    bitpool > bitcount + 1 ? 2 : 0;

            nbits[ich][isb] += n;
            bitcount += n;
        }

    for (int isb = 0; isb < nsubbands && bitcount < bitpool; isb++)
        for (int ich = 0; ich < nchannels && bitcount < bitpool; ich++) {

            int n = (nbits[ich][isb] < 16);
            nbits[ich][isb] += n;
            bitcount += n;
        }
}

/**
 * Return the sampling frequency in Hz
 */
int sbc_get_freq_hz(enum sbc_freq freq)
{
    static const int freq_hz[SBC_NUM_FREQ] = {
        [SBC_FREQ_16K ] = 16000, [SBC_FREQ_32K ] = 32000,
        [SBC_FREQ_44K1] = 44100, [SBC_FREQ_48K ] = 48000,
    };

    return freq_hz[freq];
}

/**
 * Return the frame size from a frame description
 */
unsigned sbc_get_frame_size(const struct sbc_frame *frame)
{
    if (!check_frame(frame))
        return 0;

    bool two_channels = (frame->mode != SBC_MODE_MONO);
    bool dual_mode = (frame->mode == SBC_MODE_DUAL_CHANNEL);
    bool joint_mode = (frame->mode == SBC_MODE_JOINT_STEREO);

    unsigned nbits =
        ((4 * frame->nsubbands) << two_channels) +
        ((frame->nblocks * frame->bitpool) << dual_mode) +
        ((joint_mode ? frame->nsubbands : 0));

    return SBC_HEADER_SIZE + ((nbits + 7) >> 3);
}

/**
 * Return the bitrate from a frame description
 */
unsigned sbc_get_frame_bitrate(const struct sbc_frame *frame)
{
    if (!check_frame(frame))
        return 0;

    unsigned nsamples = frame->nblocks * frame->nsubbands;
    unsigned nbits = 8 * sbc_get_frame_size(frame);

    return (nbits * sbc_get_freq_hz(frame->freq)) / nsamples;
}

/**
 * Return the bitrate from a frame description
 */
int sbc_get_frame_bps(enum sbc_freq freq)
{
    static const int freq_hz[SBC_NUM_FREQ] = {
        [SBC_FREQ_16K ] = 16000, [SBC_FREQ_32K ] = 32000,
        [SBC_FREQ_44K1] = 44100, [SBC_FREQ_48K ] = 48000,
    };

    return freq_hz[freq];
}

/**
 * Reset of the context
 */
void sbc_reset(struct sbc *sbc)
{
    *sbc = (struct sbc){ };
}


/* ----------------------------------------------------------------------------
 *  Decoding
 * ------------------------------------------------------------------------- */

/**
 * Decode the 4 bytes frame header
 * bits            Bitstream reader
 * frame           Return the frame description
 * crc             Return the CRC validating the stream, or NULL
 * return          True on success, False otherwise
 */


static __attribute__((always_inline)) inline bool decode_header(sbc_bits_t *bits, struct sbc_frame *frame, int *crc)
{
    static const enum sbc_freq dec_freq[] =
        { SBC_FREQ_16K, SBC_FREQ_32K, SBC_FREQ_44K1, SBC_FREQ_48K };

    static const enum sbc_mode dec_mode[] =
        { SBC_MODE_MONO, SBC_MODE_DUAL_CHANNEL,
          SBC_MODE_STEREO, SBC_MODE_JOINT_STEREO };

    static const enum sbc_bam dec_bam[] =
        { SBC_BAM_LOUDNESS, SBC_BAM_SNR };

    /* --- Decode header ---
     *
     * Two possible headers :
     * - Header, with syncword 0x9c (A2DP)
     * - mSBC header, with syncword 0xad (HFP) */

    SBC_WITH_BITS(bits);

    int syncword = SBC_GET_BITS("syncword", 8);
    frame->msbc = (syncword == 0xad);
    if (frame->msbc) {
        SBC_GET_BITS("reserved", 16);
        *frame = msbc_frame;
    }

    else if (syncword == 0x9c) {
        frame->freq = dec_freq[SBC_GET_BITS("sampling_frequency", 2)];
        frame->nblocks = (1 + SBC_GET_BITS("blocks", 2)) << 2;
        frame->mode = dec_mode[SBC_GET_BITS("channel_mode", 2)];
        frame->bam = dec_bam[SBC_GET_BITS("allocation_method", 1)];
        frame->nsubbands = (1 + SBC_GET_BITS("subbands", 1)) << 2;
        frame->bitpool = SBC_GET_BITS("bitpool", 8);
        // print all params
        /*printf("freq: %d\n", frame->freq);
        printf("nblocks: %d\n", frame->nblocks);
        printf("mode: %d\n", frame->mode);
        printf("bam: %d\n", frame->bam);
        printf("nsubbands: %d\n", frame->nsubbands);
        printf("bitpool: %d\n", frame->bitpool);*/


    } else
        return false;

    if (crc)
        *crc = SBC_GET_BITS("crc_check", 8);

    SBC_END_WITH_BITS();

    /* --- Check bitpool value and return --- */

    return check_frame(frame);
}

/**
 * Decode frame data
 * bits            Bitstream reader
 * frame           Frame description
 * sb_samples      Return the sub-band samples, by channels
 * sb_scale        Return the sample scaler, by track (indep. channels)
 */
static __attribute__((always_inline)) inline void decode_frame(sbc_bits_t *bits, const struct sbc_frame *frame,
    int16_t (*sb_samples)[SBC_MAX_SAMPLES], int *sb_scale)
{
    static const int range_scale[] = {
        0xFFFFFFF, 0x5555556, 0x2492492, 0x1111111,
        0x0842108, 0x0410410, 0x0204081, 0x0101010,
        0x0080402, 0x0040100, 0x0020040, 0x0010010,
        0x0008004, 0x0004001, 0x0002000, 0x0001000
    };

    /* --- Decode joint bands indications --- */

    SBC_WITH_BITS(bits);

    unsigned mjoint = 0;

    if (frame->mode == SBC_MODE_JOINT_STEREO && frame->nsubbands == 4)
    {
        unsigned v = SBC_GET_BITS("join[]", 4);
        mjoint = ((    0x00) << 3) | ((v & 0x02) << 1) |
                 ((v & 0x04) >> 1) | ((v & 0x08) >> 3)  ;

    } else if (frame->mode == SBC_MODE_JOINT_STEREO) {
        unsigned v = SBC_GET_BITS("join[]", 8);

        mjoint = ((    0x00) << 7) | ((v & 0x02) << 5) |
                 ((v & 0x04) << 3) | ((v & 0x08) << 1) |
                 ((v & 0x10) >> 1) | ((v & 0x20) >> 3) |
                 ((v & 0x40) >> 5) | ((v & 0x80) >> 7)  ;
    }

    /* --- Decode scale factors --- */

    int nchannels = 1 + (frame->mode != SBC_MODE_MONO);
    int nsubbands = frame->nsubbands;

    int scale_factors[2][SBC_MAX_SUBBANDS];
    int nbits[2][SBC_MAX_SUBBANDS];

    for (int ich = 0; ich < nchannels; ich++)
        for (int isb = 0; isb < nsubbands; isb++)
            scale_factors[ich][isb] = SBC_GET_BITS("scale_factor", 4);

    compute_nbits(frame, scale_factors, nbits);
    if (frame->mode == SBC_MODE_DUAL_CHANNEL)
        compute_nbits(frame, scale_factors + 1, nbits + 1);

    /* --- Decode samples ---
     *
     * They are unquantized according :
     *
     *                  2 sample + 1
     *   sb_sample = ( -------------- - 1 ) 2^(scf + 1)
     *                   2^nbit - 1
     *
     * A sample is coded on maximum 16 bits, and the scale factor is limited
     * to 15 bits. Thus the dynamic of sub-bands samples are 17 bits.
     * Regarding "Joint-Stereo" sub-bands, uncoupling increase the dynamic
     * to 18 bits.
     *
     * The `1 / (2^nbit - 1)` values are precalculated on 1.28 :
     *
     *   sb_sample = ((2 sample + 1) * range_scale - 2^28) / 2^shr
     *
     *   with  shr = 28 - ((scf + 1) + sb_scale)
     *         sb_scale = (15 - max(scale_factor[])) - (18 - 16)
     *
     * We introduce `sb_scale`, to limit the range on 16 bits, or increase
     * precision when the scale-factor of the frame is below 13. */

    for (int ich = 0; ich < nchannels; ich++) {
        int max_scf = 0;

        for (int isb = 0; isb < nsubbands; isb++) {
            int scf = scale_factors[ich][isb] + ((mjoint >> isb) & 1);
            if (scf > max_scf) max_scf = scf;
        }

        sb_scale[ich] = (15 - max_scf) - (17 - 16);
    }

    if (frame->mode == SBC_MODE_JOINT_STEREO)
        sb_scale[0] = sb_scale[1] =
            sb_scale[0] < sb_scale[1] ? sb_scale[0] : sb_scale[1];

    for (int iblk = 0; iblk < frame->nblocks; iblk++)
        for (int ich = 0; ich < nchannels; ich++) {
            int16_t *p_sb_samples = sb_samples[ich] + iblk*nsubbands;

            for (int isb = 0; isb < nsubbands; isb++) {
                int nbit = nbits[ich][isb];
                int scf = scale_factors[ich][isb];

                if (!nbit) { *(p_sb_samples++) = 0; continue; }

                int s = SBC_GET_BITS("audio_sample", nbit);
                s = ((s << 1) | 1) * range_scale[nbit-1];

                *(p_sb_samples++) =
                    (s - (1 << 28)) >> (28 - ((scf + 1) + sb_scale[ich]));
            }
        }

    /* --- Uncoupling "Joint-Stereo" ---
     *
     * The `Left/Right` samples are coded as :
     *   `sb_sample(left ) = sb_sample(ch 0) + sb_sample(ch 1)`
     *   `sb_sample(right) = sb_sample(ch 0) - sb_sample(ch 1)` */

    for (int isb = 0; isb < nsubbands; isb++) {

        if (((mjoint >> isb) & 1) == 0)
            continue;

        for (int iblk = 0; iblk < frame->nblocks; iblk++) {
            int16_t s0 = sb_samples[0][iblk*nsubbands + isb];
            int16_t s1 = sb_samples[1][iblk*nsubbands + isb];

            sb_samples[0][iblk*nsubbands + isb] = s0 + s1;
            sb_samples[1][iblk*nsubbands + isb] = s0 - s1;
        }
    }

    /* --- Remove padding --- */

    int padding_nbits = 8 - (sbc_tell_bits(bits) % 8);
    if (padding_nbits < 8)
        SBC_GET_FIXED("padding_bits", padding_nbits, 0);

    SBC_END_WITH_BITS();
}

/**
 * Perform a DCT on 4 samples
 * in              Subbands input samples
 * scale           Scale factor of input samples (-2 to 14)
 * out0            Output of 1st half samples
 * out1            Output of 2nd half samples
 * idx             Index of transformed samples
 */
static __attribute__((always_inline)) inline void dct4(const int16_t *in, int scale,
    int16_t (*out0)[10], int16_t (*out1)[10], int idx)
{
     /* cos(i*pi/8)  for i = [0;3], in fixed 0.13 */

    static const int16_t cos8[] = { 8192, 7568, 5793, 3135 };

    /* --- DCT of subbands samples ---
     *          ___
     *          \
     *   u[k] = /__  h(k,i) * s(i) , i = [0;n-1]  k = [0;2n-1]
     *           i
     *
     *   With  n the number of subbands (4)
     *         h(k,i) = cos( (i + 1/2) (k + n/2) pi/n )
     *
     * Note :
     *
     *     h( 2, i) =  0 et h( n-k,i) = -h(k,i)   , k = [0;n/2-1]
     *     h(12, i) = -1 et h(2n-k,i) =  h(n+k,i) , k = [1;n/2-1]
     *
     * To assist the windowing step, the 2 halves are stored in 2 buffers.
     * After scaling of coefficients, the result is saturated on 16 bits. */

    int16_t s03 = (in[0] + in[3]) >> 1, d03 = (in[0] - in[3]) >> 1;
    int16_t s12 = (in[1] + in[2]) >> 1, d12 = (in[1] - in[2]) >> 1;

    int a0 = ( (s03 - s12 ) * cos8[2] );
    int b1 = (-(s03 + s12 ) ) << 13;
    int a1 = ( d03*cos8[3] - d12 * cos8[1] );
    int b0 = (-d03*cos8[1] - d12 * cos8[3] );

    int shr = 12 + scale;

    a0 = (a0 + (1 << (shr-1))) >> shr;  b0 = (b0 + (1 << (shr-1))) >> shr;
    a1 = (a1 + (1 << (shr-1))) >> shr;  b1 = (b1 + (1 << (shr-1))) >> shr;

    out0[0][idx] = SBC_SAT16( a0);  out0[3][idx] = SBC_SAT16(-a1);
    out0[1][idx] = SBC_SAT16( a1);  out0[2][idx] = SBC_SAT16(  0);

    out1[0][idx] = SBC_SAT16(-a0);  out1[3][idx] = SBC_SAT16( b0);
    out1[1][idx] = SBC_SAT16( b0);  out1[2][idx] = SBC_SAT16( b1);
}

/**
 * Perform a DCT on 8 samples
 * in              Subbands input samples
 * scale           Scale factor of input samples (-2 to 14)
 * out0            Output of 1st half samples
 * out1            Output of 2nd half samples
 * idx             Index of transformed samples
 */
static __attribute__((always_inline)) inline void dct8(const int16_t *in, int scale,
    int16_t (*out0)[10], int16_t (*out1)[10], int idx)
{
     /* cos(i*pi/16)  for i = [0;7], in fixed 0.13 */

    static const int16_t cos16[] =
        { 8192, 8035, 7568, 6811, 5793, 4551, 3135, 1598 };

    /* --- DCT of subbands samples ---
     *          ___
     *          \
     *   u[k] = /__  h(k,i) * s(i) , i = [0;n-1]  k = [0;2n-1]
     *           i
     *
     *   With  n the number of subbands (8)
     *         h(k,i) = cos( (i + 1/2) (k + n/2) pi/n )
     *
     *
     *
     * Note :
     *
     *     h( 4, i) =  0 et h( n-k,i) = -h(k,i)   , k = [0;n/2-1]
     *     h(12, i) = -1 et h(2n-k,i) =  h(n+k,i) , k = [1;n/2-1]
     *
     * To assist the windowing step, the 2 halves are stored in 2 buffers.
     * After scaling of coefficients, the result is saturated on 16 bits. */

    int16_t s07 = (in[0] + in[7]) >> 1, d07 = (in[0] - in[7]) >> 1;
    int16_t s16 = (in[1] + in[6]) >> 1, d16 = (in[1] - in[6]) >> 1;
    int16_t s25 = (in[2] + in[5]) >> 1, d25 = (in[2] - in[5]) >> 1;
    int16_t s34 = (in[3] + in[4]) >> 1, d34 = (in[3] - in[4]) >> 1;

    int a0 = ( (s07 + s34) - (s25 + s16) ) * cos16[4];
    int b3 = (-(s07 + s34) - (s25 + s16) ) << 13;
    int a2 = ( (s07 - s34) * cos16[6] + (s25 - s16) * cos16[2] );
    int b1 = ( (s34 - s07) * cos16[2] + (s25 - s16) * cos16[6] );
    int a1 = ( d07*cos16[5] - d16*cos16[1] + d25*cos16[7] + d34*cos16[3] );
    int b2 = (-d07*cos16[1] - d16*cos16[3] - d25*cos16[5] - d34*cos16[7] );
    int a3 = ( d07*cos16[7] - d16*cos16[5] + d25*cos16[3] - d34*cos16[1] );
    int b0 = (-d07*cos16[3] + d16*cos16[7] + d25*cos16[1] + d34*cos16[5] );

    int shr = 12 + scale;

    a0 = (a0 + (1 << (shr-1))) >> shr;  b0 = (b0 + (1 << (shr-1))) >> shr;
    a1 = (a1 + (1 << (shr-1))) >> shr;  b1 = (b1 + (1 << (shr-1))) >> shr;
    a2 = (a2 + (1 << (shr-1))) >> shr;  b2 = (b2 + (1 << (shr-1))) >> shr;
    a3 = (a3 + (1 << (shr-1))) >> shr;  b3 = (b3 + (1 << (shr-1))) >> shr;

    out0[0][idx] = SBC_SAT16( a0);  out0[7][idx] = SBC_SAT16(-a1);
    out0[1][idx] = SBC_SAT16( a1);  out0[6][idx] = SBC_SAT16(-a2);
    out0[2][idx] = SBC_SAT16( a2);  out0[5][idx] = SBC_SAT16(-a3);
    out0[3][idx] = SBC_SAT16( a3);  out0[4][idx] = SBC_SAT16(  0);

    out1[0][idx] = SBC_SAT16(-a0);  out1[7][idx] = SBC_SAT16( b0);
    out1[1][idx] = SBC_SAT16( b0);  out1[6][idx] = SBC_SAT16( b1);
    out1[2][idx] = SBC_SAT16( b1);  out1[5][idx] = SBC_SAT16( b2);
    out1[3][idx] = SBC_SAT16( b2);  out1[4][idx] = SBC_SAT16( b3);
}


#define SAMPLE_SAT(v) (int16_t)\
    ( (v) > (272*4) ? (272*4) : (v))


extern volatile uint8_t VolumeShift;

/**
 * Apply window on reconstructed samples
 * in, n           Reconstructed samples and number of subbands
 * window          Window coefficients
 * offset          Offset of coefficients for each samples
 * out             Output adress of PCM samples
 * pitch           Number of PCM samples between two consecutive
 */
static __attribute__((always_inline)) inline void apply_window(const int16_t (*in)[10], int n,
    const int16_t (*window)[2*10], int offset, int16_t *out)
{
    const int16_t *u = (const int16_t *)in;

    const uint8_t shift = VolumeShift; // make sure it's only read once

    for (int i = 0; i < n; i++) {
        const int16_t *w = window[i] + offset;
        int s;

        s  = *(u++) * *(w++);  s += *(u++) * *(w++);
        s += *(u++) * *(w++);  s += *(u++) * *(w++);
        s += *(u++) * *(w++);  s += *(u++) * *(w++);
        s += *(u++) * *(w++);  s += *(u++) * *(w++);
        s += *(u++) * *(w++);  s += *(u++) * *(w++);

        *out = SAMPLE_SAT((SBC_SAT16((s + (1 << 12)) >> 13) >> 6 >> shift) + (136*4));  out += 1;
    }
}

/**
 * Synthesize samples of a 4 subbands block
 * state           Previous transformed samples of the channel
 * in              Sub-band samples
 * scale           Scale factor of samples
 * out             Output adress of PCM samples
 * pitch           Number of samples between two consecutive
 */
static __attribute__((always_inline)) inline void sbc_synthesize_4_c(struct sbc_dstate *state,
    const int16_t *in, int scale, int16_t *out)
{
    /* --- Windowing coefficients (fixed 2.13) ---
     *
     * The table is duplicated and transposed to fit the circular
     * buffer of reconstructed samples */

    static const int16_t window[4][2*10] =  {
        {   0, -126,  -358, -848, -4443, -9644, 4443,  -848,  358, -126,
            0, -126,  -358, -848, -4443, -9644, 4443,  -848,  358, -126 },

        { -18, -128,  -670, -201, -6389, -9235, 2544, -1055,  100,  -90,
          -18, -128,  -670, -201, -6389, -9235, 2544, -1055,  100,  -90 },

        { -49,  -61,  -946,  944, -8082, -8082,  944,  -946,  -61,  -49,
          -49,  -61,  -946,  944, -8082, -8082,  944,  -946,  -61,  -49 },

        { -90,  100, -1055, 2544, -9235, -6389, -201,  -670, -128,  -18,
          -90,  100, -1055, 2544, -9235, -6389, -201,  -670, -128,  -18 }
    };

    /* --- IDCT and windowing --- */

    int dct_idx = state->idx ? 10 - state->idx : 0, odd = dct_idx & 1;

    dct4(in, scale, state->v[odd], state->v[!odd], dct_idx);
    apply_window(state->v[odd], 4, window, state->idx, out);

    state->idx = state->idx < 9 ? state->idx + 1 : 0;
}

/**
 * Synthesize samples of a 8 subbands block
 * state           Previous transformed samples of the channel
 * sb_samples      Sub-band samples
 * sb_scale        Scale factor of samples (-2 to 14)
 * out             Output adress of PCM samples
 * pitch           Number of PCM samples between two consecutive
 */
static __attribute__((always_inline)) inline void sbc_synthesize_8_c(struct sbc_dstate *state,
    const int16_t *in, int scale, int16_t *out)
{
    /* --- Windowing coefficients (fixed 2.13) ---
     *
     * The table is duplicated and transposed to fit the circular
     * buffer of reconstructed samples */

    static const int16_t window[8][2*10] = {
        {    0, -132,  -371, -848, -4456, -9631, 4456,  -848,  371, -132,
             0, -132,  -371, -848, -4456, -9631, 4456,  -848,  371, -132 },

        {  -10, -138,  -526, -580, -5438, -9528, 3486, -1004,  229, -117,
           -10, -138,  -526, -580, -5438, -9528, 3486, -1004,  229, -117 },

        {  -22, -131,  -685, -192, -6395, -9224, 2561, -1063,  108,  -97,
           -22, -131,  -685, -192, -6395, -9224, 2561, -1063,  108,  -97 },

        {  -36, -106,  -835,  322, -7287, -8734, 1711, -1042,   12,  -75,
           -36, -106,  -835,  322, -7287, -8734, 1711, -1042,   12,  -75 },

        {  -54,  -59,  -960,  959, -8078, -8078,  959,  -960,  -59,  -54,
           -54,  -59,  -960,  959, -8078, -8078,  959,  -960,  -59,  -54 },

        {  -75,   12, -1042, 1711, -8734, -7287,  322,  -835, -106,  -36,
           -75,   12, -1042, 1711, -8734, -7287,  322,  -835, -106,  -36 },

        {  -97,  108, -1063, 2561, -9224, -6395, -192,  -685, -131,  -22,
           -97,  108, -1063, 2561, -9224, -6395, -192,  -685, -131,  -22 },

        { -117,  229, -1004, 3486, -9528, -5438, -580,  -526, -138,  -10,
          -117,  229, -1004, 3486, -9528, -5438, -580,  -526, -138,  -10 }
    };

    /* --- IDCT and windowing --- */

    int dct_idx = state->idx ? 10 - state->idx : 0, odd = dct_idx & 1;

    dct8(in, scale, state->v[odd], state->v[!odd], dct_idx);
    apply_window(state->v[odd], 8, window, state->idx, out);

    state->idx = state->idx < 9 ? state->idx + 1 : 0;
}

/**
 * Synthesize samples of a channel
 * state           Previous transformed samples of the channel
 * nblocks         Number of blocks (4, 8, 12 or 16)
 * nsubbands       Number of subbands (4 or 8)
 * in              Sub-band input samples
 * scale           Scale factor of samples
 * out             Output adress of PCM samples
 * pitch           Number of PCM samples between two consecutive
 */
static __attribute__((always_inline)) inline void synthesize(
    struct sbc_dstate *state, int nblocks, int nsubbands,
    const int16_t *in, int scale, int16_t *out)
{
    for (int iblk = 0; iblk < nblocks; iblk++) {

        if (nsubbands == 4)
            ASM(sbc_synthesize_4)(state, in, scale, out);
        else
            ASM(sbc_synthesize_8)(state, in, scale, out);

        in += nsubbands;
        out += nsubbands;
    }
}

/**
 * Probe data and return frame description
 */
int sbc_probe(const void *data, struct sbc_frame *frame)
{
    sbc_bits_t bits = {};

    sbc_setup_bits(&bits, SBC_BITS_READ, (void *)data, SBC_HEADER_SIZE);
    int ret = !decode_header(&bits, frame, NULL) ||
           sbc_bits_error(&bits) ? -1 : 0;

    return ret;
}

/**
 * Decode a frame
 */


int sbc_decode(struct sbc *sbc,
    const void *data, unsigned size, struct sbc_frame *frame,
    int16_t *pcml, int16_t *pcmr)
{
    sbc_bits_t bits;
    int crc;

    /* --- Decode the frame header --- */

    if (data) {

        if (size < SBC_HEADER_SIZE)
            return -1;

        sbc_setup_bits(&bits, SBC_BITS_READ, (void *)data, SBC_HEADER_SIZE);
        if (!decode_header(&bits, frame, &crc) || sbc_bits_error(&bits))
            return -1;

        if (size < sbc_get_frame_size(frame) /*||
            compute_crc(frame, (const uint8_t*)data, size) != crc*/)
            return -1;
    }

    /* --- Decode the frame data --- */
    int16_t alignas(sizeof(int)) sb_samples[2][SBC_MAX_SAMPLES];
    int sb_scale[2];

    if (data) {

        sbc_setup_bits(&bits, SBC_BITS_READ,
            (void *)((uintptr_t)data + SBC_HEADER_SIZE),
            sbc_get_frame_size(frame) - SBC_HEADER_SIZE);
        
        decode_frame(&bits, frame, sb_samples, sb_scale);

        sbc->nchannels = 1 + (frame->mode != SBC_MODE_MONO);
        sbc->nblocks = frame->nblocks;
        sbc->nsubbands = frame->nsubbands;

    } else {

        int nsamples = sbc->nblocks * sbc->nsubbands;

        for (int ich = 0; ich < sbc->nchannels; ich++) {
            memset(sb_samples[ich], 0, nsamples * sizeof(int16_t));
            sb_scale[ich] = 0;
        }
    }

    synthesize(&sbc->dstates[0], sbc->nblocks, sbc->nsubbands,
        sb_samples[0], sb_scale[0], pcml);

    if (frame->mode != SBC_MODE_MONO)
        synthesize(&sbc->dstates[1], sbc->nblocks, sbc->nsubbands,
            sb_samples[1], sb_scale[1], pcmr);


    return 0;
}

