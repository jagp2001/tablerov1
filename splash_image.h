#pragma once

#include <Arduino.h>

// Reemplaza el contenido del arreglo con la imagen convertida a RGB565.
// Tip: usa "TFT_eSPI > Tools > ImageConverter" y exporta como array RGB565.
const uint16_t SPLASH_WIDTH = 320;
const uint16_t SPLASH_HEIGHT = 240;
const uint16_t splashImage[SPLASH_WIDTH * SPLASH_HEIGHT] PROGMEM = {0};
