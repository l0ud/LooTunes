#pragma once

#include "button.h"


class Controller {
    public:
        static enum class MState {
            SENSOR, // light sensor state
            FORCED_ON,
            FORCED_OFF, // forced off state
            LAST_ELEMENT // marker for calculation
        } m_state;

        static enum class PState {
            NOT_PLAYING,
            PLAYING,
        } p_state;

        static bool init();
        static void main();
        static void on_button_press(BTN::ID id);
        static void on_light_sensor(uint16_t value);
    private:
        static void change_main_state(MState new_state);
        static void change_playing_state(PState new_state);
        static void set_thresholds_for_state();
        static void play_file(const char* filename);

};
