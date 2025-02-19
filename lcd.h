#pragma once

#include "gui/draw_vector.h"
#include "gui/element_pool.h"

class LCD {
    public:
        static constexpr auto Width = 320;
        static constexpr auto Height = 170;

        static void init();
        static void on();
        static void frame_command();
        static void EndFrame();
        static void DrawLine(uint16_t *line);
        static void ConfigChanged();
        static void LightIntensityChanged(bool isBright);
        static void fillBlack();
        static void fillRed();
        static void listen() {
            cs_set();
        }
        static void stop_listening() {
            cs_reset();
        }
    private:
        static void pwm_init();
        static void cs_set();
        static void cs_reset();
        static void dc_set();
        static void dc_reset();
        static void command(uint8_t c);
        static void command2(uint8_t c, uint16_t d1, uint16_t d2);
        static void byte_write(uint8_t d);
};