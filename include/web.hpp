#pragma once

#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

#include "ws2812b_8x8.hpp"

namespace pixelbox
{
  namespace web
  {
    typedef void (*routeCallbackFunction)(AsyncWebServerRequest*);
    typedef void (*voidcb)(void);

    bool set_displayed_image(String name);
    bool get_displayed_image(String& filename);
    void select_next_image(String name);  
    bool del_image(String name);
    void add_updated_cb(voidcb callback);

    void setup();
  }
}