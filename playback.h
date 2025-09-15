#pragma once

#include "button.h"

namespace Controller {
    void on_button_press(BTN::ID id);
    void on_light_sensor(uint16_t value);

    void init();
    bool init_sd();
    bool main();

};
