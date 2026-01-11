/*
 * OLED Driver Helper for Task 6
 * Uses Adafruit SSD1306 library
 */

#ifndef OLED_DRIVER_H
#define OLED_DRIVER_H

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDR      0x3C
#define I2C_SDA        21
#define I2C_SCL        22

#endif
