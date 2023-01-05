#pragma once

#include <OneButton.h>

#define BUTTON_GPIO_PIN        0
#define BUTTON_ACTIVE_LOW      true
#define BUTTON_PULL_UP_ENABLE  false

namespace pixelbox
{
  namespace button
  {     
    void setup(callbackFunction click_callback);
    void loop();  
  }
}
