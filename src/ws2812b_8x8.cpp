#include "ws2812b_8x8.hpp"

#include <FastLED.h>
#include <arduino-timer.h>
#include "anim.hpp"
#include "Hash.h"

namespace pixelbox
{
  namespace ws2812b_8x8
  {
    CRGB out[WS_LED_NUM];             //framebuffer, FastLED will display this
    bool on = true;                   //enable/disable display
    anim::animation_s* anim = NULL;   //pointer of animation to be displayed

    Timer timer = Timer<1, millis>(); //ms timer for rendering

    //locally used funcs
    bool render(void* data);
    void render_next_anim_frame();

    void set(CRGB *in)
    {
      if(in == NULL) return;
      ws2812b_8x8::anim = NULL;
      timer.cancel();
      timer.every(33, render);
      memcpy(out, in, WS_LED_NUM * 3);
      FastLED.show();
    }

    void set(anim::animation_s* anim)
    {
      ws2812b_8x8::anim = anim;
      render_next_anim_frame();
      FastLED.show();
    }

    void set_color(CRGB color)
    {
      ws2812b_8x8::anim = NULL;
      timer.cancel();
      timer.every(33, render);
      fill_solid(out, WS_LED_NUM, color);
      FastLED.show();
    }

    void set_brightness(uint8_t value)
    {
      FastLED.setBrightness(value);
    }

    void set_brightness_percent(uint8_t percent)
    {
      if(percent > 100) percent = 100;
      FastLED.setBrightness(percent * 255 / 100);
    }

    void set_max_current(uint32 current_ma)
    {
      if(current_ma > 3000) current_ma = 3000;
      FastLED.setMaxPowerInVoltsAndMilliamps(5, current_ma);
    }

    void set_enable(bool on)
    {
      ws2812b_8x8::on = on;
      if(!on)
      {
        ws2812b_8x8::anim = NULL;
        fill_solid(out, WS_LED_NUM, CRGB::Black);
        FastLED.show();
      }
    }

    void render_next_anim_frame()
    {
      if(!anim) return;

      //loop the animation if reached the end
      if(anim->frame_index >= anim->frames_size) anim->frame_index = 0;

      //set the timer at the next frame transition
      timer.cancel();
      timer.every(anim->frames[anim->frame_index].delay_ms, render);
      
      //overcopy protection & copy pixel data to the frambuffer
      uint16_t pixels_to_copy = anim->frames[anim->frame_index].pixels_size;
      if(pixels_to_copy > WS_LED_NUM) pixels_to_copy = WS_LED_NUM;
      memcpy(out, anim->frames[anim->frame_index].pixels, pixels_to_copy * 3);

      //increment the frame index for next iteration
      anim->frame_index++;
    }

    bool render(void* data)
    {
      render_next_anim_frame(); //returns immediately if no animation is set
      FastLED.show();
      return true;
    }

    void setup()
    {
      FastLED.addLeds<WS2812B, WS_DATA_PIN, GRB>(out, WS_LED_NUM);
      FastLED.setBrightness(64);
      fill_solid(out, WS_LED_NUM, CHSV(0,0,0));
      FastLED.show();
      timer.every(33, render); //set the render timer @30 FPS
      anim = NULL;
    }

    void loop()
    {
      timer.tick();
    }
  };
};