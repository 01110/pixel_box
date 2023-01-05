#pragma once

#include <FastLED.h>
#include <arduino-timer.h>
#include "anim.hpp"
#include "Hash.h"

#define WS_LED_WIDTH  8
#define WS_LED_HEIGHT 8
#define WS_LED_NUM    (WS_LED_WIDTH * WS_LED_HEIGHT)
#define WS_DATA_PIN   2

namespace pixelbox
{
  namespace ws2812b_8x8
  {
    //set data to be displayed
    void set(CRGB *in); //set image 
    void set(anim::animation_s* anim); //set animation
    void set_color(CRGB color); //set color

    //set display parameters
    void set_brightness(uint8_t value);
    void set_brightness_percent(uint8_t percent);
    void set_max_current(uint32 current_ma);
    void set_enable(bool on);

    void setup();
    void loop();
  };
};