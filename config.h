#include "py32f0xx.h"
#include "py32f0xx_hal.h"
#include <stdbool.h>
#include <stdlib.h>

#define LCD_BLK_PIN GPIO_PIN_0 // GPIOA
#define LCD_SCK_PIN GPIO_PIN_1 // GPIOA
#define LCD_MOSI_PIN GPIO_PIN_12 // GPIOA
#define LCD_DC_PIN GPIO_PIN_6 // GPIOA

#define LCD_CS_PIN GPIO_PIN_0 //GPIOB
#define LCD_RES_PIN GPIO_PIN_1 //GPIOB
