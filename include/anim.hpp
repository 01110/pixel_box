#pragma once

#include <FastLED.h>

#define FRAME_ALLOCATION_SIZE 4

namespace pixelbox
{
  namespace anim
  {
    typedef struct frame_s   //one frame of an animation, holding pixel data
    {
      uint32_t delay_ms;     //how long this frame should be displayed
      uint32_t x;            //starting X coord of the frame (based on GIF partial refresh)
      uint32_t y;            //starting Y coord of the frame (based on GIF partial refresh)
      CRGB* pixels;          //pixel array pointer
      uint32_t pixels_size;  //pixel array size
    }frame_s;

    typedef struct animation_s   //animation consisting multiple frames
    {
      frame_s* frames;            //frame array pointer
      uint32_t frames_size;       //frame array used size 
      uint32_t frames_allocated;  //frame array allocated size
      uint32_t frame_index;       //actual frame index in the animation
    }animation_s;
    
    bool add_frame(animation_s* anim, uint32_t delay_ms, uint32_t x, uint32_t y, CRGB* pixels, uint32_t pixels_size); //add a frame to an animation and copy associated data (dynamic mem allocation, using calloc/realloc)
    bool animation_init(animation_s* anim); //init animation struct, reset if it contains data (dynamic mem deallocation, using free)
  } 
}
