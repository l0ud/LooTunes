#pragma once

#include <cstdint>

class Config {
public:
    // Enumerations for specific configuration parameters with uint8_t underlying type
    enum class LightMode : uint8_t {
        Disabled = 0,
        Normal = 1,
        Reversed = 2
    };

    enum class UsbMode : uint8_t {
        AlwaysOff = 0,
        AlwaysOn = 1,
        OnPlayback = 2
    };

    // Bitmask for save options: track, directory, and mode
    enum class SaveState : uint8_t {
        Disabled      = 0x00,
        SaveTrack     = 0x01,
        SaveDirectory = 0x02,
        SaveMode      = 0x04
    };

    // Constructor
    Config();

    // Configuration parameters
    uint8_t random_mode;         // Random playback mode
    uint32_t seed;            // Seed for randomization

    LightMode light_mode;       // Light mode
    uint16_t on_threshold;        // Light threshold to power on (normal mode)
    uint16_t off_threshold;       // Light threshold to power off (normal mode)

    UsbMode usb_mode;           // USB power mode

    uint8_t fade_in;            // Fade-in step time in ms
    uint8_t fade_out;           // Fade-out step time in ms

    SaveState save_state;         // Bitmask for save options: track, directory, and mode

    uint8_t jump_next_dir;

    inline bool saving_enabled(SaveState mode) const {
        return (static_cast<uint8_t>(save_state) & static_cast<uint8_t>(mode)) != 0;
    }

    inline void enable_saving(SaveState mode) {
        save_state = static_cast<SaveState>(static_cast<uint8_t>(save_state) | static_cast<uint8_t>(mode));
    }

    // Load configuration from file
    __attribute__((noinline)) bool load_from_file(const char* filename);

private:
    // Optional: helper methods for parsing
};

extern Config CFG;
