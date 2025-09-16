#include "config.h"
#include "petitfat/source/pff.h"

#include <cstring>
#include <cstdlib>

//namespace {
    // not including cstdlib because of __sf overhead
    /*
    int atoi(const char* str) {
        int result = 0;
        while (*str >= '0' && *str <= '9') {
            result = result * 10 + (*str - '0');
            str++;
        }
        return result;
    }

    int strcmp(const char* s1, const char* s2) {
        while (*s1 && (*s1 == *s2)) {
            s1++;
            s2++;
        }
        return static_cast<unsigned char>(*s1) - static_cast<unsigned char>(*s2);
    }

    //implement strchr
    char* strchr(const char* str, char ch) {
        while (*str) {
            if (*str == ch) {
                return const_cast<char*>(str);
            }
            str++;
        }
        return nullptr;
    }*/

    /*

    // Static table of handlers
    struct KeyHandler {
        const char* key;
        void (*apply)(Config& cfg, const char* value);
    };

    // Handler functions
    static void set_uint8(uint8_t& field, const char* value) {
        field = static_cast<uint8_t>(atoi(value));
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

    constexpr KeyHandler key_handlers[] = {
        { "random_min", [](Config& cfg, const char* val) { set_uint8(cfg.random_min, val); } },
        { "random_max", [](Config& cfg, const char* val) { set_uint8(cfg.random_max, val); } },
        { "light_mode", set_light_mode },
        { "on_threshold", [](Config& cfg, const char* val) { set_uint8(cfg.on_threshold, val); } },
        { "off_threshold", [](Config& cfg, const char* val) { set_uint8(cfg.off_threshold, val); } },
        { "usb_mode", set_usb_mode },
        { "fade_in", [](Config& cfg, const char* val) { set_uint8(cfg.fade_in, val); } },
        { "fade_out", [](Config& cfg, const char* val) { set_uint8(cfg.fade_out, val); } },
    };

    */
//}

Config::Config()
    : random_mode(1),
      seed(0),
      light_mode(LightMode::Normal),
      // threshold range 0 - 0xfff
      // closer to 0 the more light is present
      on_threshold(0xaff),
      off_threshold(0xcff),
      usb_mode(UsbMode::OnPlayback),
      fade_in(10),
      fade_out(10),
      save_state(SaveState::Disabled),
      jump_next_dir(0)
{
}

/*
bool Config::load_from_file(const char* filename) {
   
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
                        if (strcmp(key, handler.key) == 0) {
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
}*/


__attribute__((noinline)) bool Config::load_from_file(const char* filename) {
    FRESULT res = pf_open(filename);
    if (res != FR_OK) {
        return false;
    }

    // Buffer to read line by line
    static constexpr int LINE_MAX_LEN = 128;
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
            // End of line or buffer full: null-terminate and process line
            *ptr = '\0';

            // Skip empty lines or comments
            char* trimmed = line;
            while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

            if (*trimmed != ';' && *trimmed != '\0') {
                // Process key=value here
                char* equal_sign = strchr(trimmed, '=');
                if (equal_sign != nullptr) {
                    *equal_sign = '\0';
                    char* key = trimmed;
                    char* value = equal_sign + 1;

                    // todo: might remove trailing spaces from key and value if needed
                    
                    // Parse and assign config parameters
                    if (strcmp(key, "random_mode") == 0) {
                        random_mode = static_cast<uint8_t>(atoi(value));
                    }
                    else if (strcmp(key, "seed") == 0) {
                        seed = static_cast<uint32_t>(atoi(value));
                    }
                    else if (strcmp(key, "light_mode") == 0) {
                        int v = atoi(value);
                        if (v >= 0 && v <= 2) {
                            light_mode = static_cast<LightMode>(v);
                        }
                    }
                    else if (strcmp(key, "on_threshold") == 0) {
                        on_threshold = static_cast<uint16_t>(atoi(value));
                        on_threshold = 0xfff - on_threshold; // invert threshold
                    }
                    else if (strcmp(key, "off_threshold") == 0) {
                        off_threshold = static_cast<uint16_t>(atoi(value));
                        off_threshold = 0xfff - off_threshold; // invert threshold
                    }
                    else if (strcmp(key, "usb_mode") == 0) {
                        int v = atoi(value);
                        if (v >= 0 && v <= 2) {
                            usb_mode = static_cast<UsbMode>(v);
                        }
                    }
                    else if (strcmp(key, "fade_in") == 0) {
                        fade_in = static_cast<uint8_t>(atoi(value));
                    }
                    else if (strcmp(key, "fade_out") == 0) {
                        fade_out = static_cast<uint8_t>(atoi(value));
                    }
                    else if (strcmp(key, "save_directory") == 0) {
                        bool enabled = (atoi(value) != 0);
                        if (enabled) {
                            enable_saving(SaveState::SaveDirectory);
                        }
                    }
                    else if (strcmp(key, "save_track") == 0) {
                        bool enabled = (atoi(value) != 0);
                        if (enabled) {
                            enable_saving(SaveState::SaveDirectory);
                            enable_saving(SaveState::SaveTrack);
                        }
                    }
                    else if (strcmp(key, "save_mode") == 0) {
                        bool enabled = (atoi(value) != 0);
                        if (enabled) {
                            enable_saving(SaveState::SaveMode);
                        }
                    }
                    else if (strcmp(key, "jump_next_dir") == 0) {
                        jump_next_dir = static_cast<uint8_t>(atoi(value));
                    }
                }
            }

            // Reset for next line
            ptr = line;
            line_pos = 0;

            // Skip consecutive \r\n pairs if needed (optional)
        } else {
            *ptr++ = c;
            line_pos++;
        }
    }

    return true;
}


Config CFG;