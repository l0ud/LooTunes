/*
 * Copyright (c) 2025 Przemysław Romaniak
 * 
 * This source code is licensed under the MIT License.
 * See the LICENSE file in the root directory for details.
*/

#include "config.h"
#include "petitfat/source/pff.h"
#include <cstring>
namespace {
    // not including cstdlib because of __sf overhead
    int atoi(const char* str) {
        int result = 0;
        while (*str >= '0' && *str <= '9') {
            result = result * 10 + (*str - '0');
            str++;
        }
        return result;
    }

    char* strchr(const char* str, char ch) {
        while (*str) {
            if (*str == ch) {
                return const_cast<char*>(str);
            }
            str++;
        }
        return nullptr;
    }

    // Static table of handlers
    struct KeyHandler {
        const char* key;
        void (*apply)(Config& cfg, const char* value);
    };

    // Handler functions
    static void set_uint8(uint8_t& field, const char* value) {
        field = static_cast<uint8_t>(atoi(value));
    }

    static void set_uint32(uint32_t& field, const char* value) {
        field = static_cast<uint32_t>(atoi(value));
    }

    static void set_uint16_inverted(uint16_t& field, const char* value) {
        field = 0xfff - static_cast<uint16_t>(atoi(value)); // invert threshold
    }

    static void set_light_mode(Config& cfg, const char* value) {
        int v = atoi(value);
        if (v >= 0 && v <= 2)
            cfg.light_mode = static_cast<Config::LightMode>(v);
    }

    static void set_usb_mode(Config& cfg, const char* value) {
        int v = atoi(value);
        if (v >= 0 && v <= 2)
            cfg.usb_mode = static_cast<Config::UsbMode>(v);
    }

    static void set_save_directory(Config& cfg, const char* value) {
        if (atoi(value) != 0) {
            cfg.enable_saving(Config::SaveState::SaveDirectory);
        }
    }

    static void set_save_track(Config& cfg, const char* value) {
        if (atoi(value) != 0) {
            cfg.enable_saving(Config::SaveState::SaveDirectory);
            cfg.enable_saving(Config::SaveState::SaveTrack);
        }
    }

    static void set_save_mode(Config& cfg, const char* value) {
        if (atoi(value) != 0) {
            cfg.enable_saving(Config::SaveState::SaveMode);
        }
    }

    constexpr KeyHandler key_handlers[] = {
        { "random_mode", [](Config& cfg, const char* val) { set_uint8(cfg.random_mode, val); } },
        { "seed", [](Config& cfg, const char* val) { set_uint32(cfg.seed, val); } },
        { "light_mode", set_light_mode },
        { "on_threshold", [](Config& cfg, const char* val) { set_uint16_inverted(cfg.on_threshold, val); } },
        { "off_threshold", [](Config& cfg, const char* val) { set_uint16_inverted(cfg.off_threshold, val); } },
        { "usb_mode", set_usb_mode },
        { "fade_in", [](Config& cfg, const char* val) { set_uint8(cfg.fade_in, val); } },
        { "fade_out", [](Config& cfg, const char* val) { set_uint8(cfg.fade_out, val); } },
        { "save_directory", set_save_directory },
        { "save_track", set_save_track },
        { "save_mode", set_save_mode },
        { "jump_next_dir", [](Config& cfg, const char* val) { set_uint8(cfg.jump_next_dir, val); } },
        { "instant_mode_change", [](Config& cfg, const char* val) { set_uint8(cfg.instant_mode_change, val); } },
    };

}

Config::Config()
    : random_mode(1),
      seed(0),
      light_mode(LightMode::Normal),
      // threshold range 0 - 0xfff
      // closer to 0 the more light is present
      on_threshold(0xaff),
      off_threshold(0xcff),
      usb_mode(UsbMode::OnPlayback),
      fade_in(50),
      fade_out(100),
      save_state(SaveState::Disabled),
      jump_next_dir(0),
      instant_mode_change(0)
{
}

__attribute__((noinline)) bool Config::load_from_file(const char* filename) {
   
    FRESULT res = pf_open(filename);
    if (res != FR_OK) {
        return false;
    }

    constexpr int LINE_MAX_LEN = 128;
    char line[LINE_MAX_LEN];
    UINT bytesRead = 0;
    char* ptr = line;
    int line_pos = 0;

    while (true) {
        char c;
        res = pf_read(&c, 1, &bytesRead);
        if (res != FR_OK || bytesRead == 0) {
            break; // EOF or error
        }

        if (c == '\n' || c == '\r' || line_pos >= LINE_MAX_LEN - 1) {
            *ptr = '\0';

            // Trim leading space
            char* trimmed = line;

            while (*trimmed == ' ' || *trimmed == '\t') trimmed++;


            if (*trimmed != ';' && *trimmed != '\0') {
                char* equal_sign = strchr(trimmed, '=');

                if (equal_sign) {
                    *equal_sign = '\0';
                    char* key = trimmed;
                    char* value = equal_sign + 1;

                    // Match key
                    for (const auto& handler : key_handlers) {
                        if (std::strcmp(key, handler.key) == 0) {
                            handler.apply(*this, value);
                            break;
                        }
                    }
                }
            }

            // Prepare for next line
            ptr = line;
            line_pos = 0;
        } else {
            *ptr++ = c;
            line_pos++;
        }
    }

    return true;
}

Config CFG;