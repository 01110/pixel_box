#include <Arduino.h>
#include <FastLED.h>
#include <LittleFS.h>

#include "ws2812b_8x8.hpp"
#include "web.hpp"
#include "wifi_manager.hpp"
#include "button.hpp"
#include "state_machine.hpp"

void setup()
{  
  pixelbox::ws2812b_8x8::setup();
  pixelbox::wifi_manager::setup();
  pixelbox::web::setup();
  pixelbox::state_machine::setup();  
  pixelbox::button::setup(pixelbox::state_machine::click_cb);
  pixelbox::web::add_updated_cb(pixelbox::state_machine::image_updated);  
}

void loop()
{
  pixelbox::ws2812b_8x8::loop();
  pixelbox::wifi_manager::loop();
  pixelbox::button::loop();
}