#include "button.hpp"

#include <OneButton.h>

namespace pixelbox
{
  namespace button
  {       
    OneButton btn = OneButton(BUTTON_GPIO_PIN, BUTTON_ACTIVE_LOW, BUTTON_PULL_UP_ENABLE); 

    void setup(callbackFunction click_callback)
    {
      btn.attachClick(click_callback);
    }

    void loop()
    {
      btn.tick();
    }    
  }
}
