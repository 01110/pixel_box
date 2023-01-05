#include "anim.hpp"
#include <FastLED.h>

namespace pixelbox
{
  namespace anim
  {    
    bool add_frame(animation_s* anim, uint32_t delay_ms, uint32_t x, uint32_t y, CRGB* pixels, uint32_t pixels_size)
    {
      if(!anim) return false;

      //alloc memory for frame data if necessary
      if(anim->frames == NULL)
      {
        anim->frames = (frame_s*)calloc(FRAME_ALLOCATION_SIZE, sizeof(frame_s));
        if(anim->frames == NULL) return false;
        anim->frames_size = 0;
        anim->frames_allocated = FRAME_ALLOCATION_SIZE;
      }

      if(anim->frames_size == anim->frames_allocated)
      {
        anim->frames = (frame_s*)realloc(anim->frames, anim->frames_allocated + FRAME_ALLOCATION_SIZE * sizeof(frame_s));
        if(anim->frames == NULL) return false;
        memset(anim->frames + anim->frames_size, 0, FRAME_ALLOCATION_SIZE * sizeof(frame_s));
        anim->frames_allocated += FRAME_ALLOCATION_SIZE;
      }

      //copy frame data
      frame_s* new_frame = &anim->frames[anim->frames_size];
      new_frame->delay_ms = delay_ms;
      new_frame->x = x;
      new_frame->y = y;
      new_frame->pixels = (CRGB*) calloc(pixels_size, sizeof(CRGB));
      if(new_frame->pixels == NULL) return false;
      memcpy(new_frame->pixels, pixels, pixels_size * sizeof(CRGB));
      new_frame->pixels_size = pixels_size;
      
      //update frames size
      anim->frames_size++;

      return true;
    }

    bool animation_init(animation_s* anim)
    {
      if(!anim) return false;
      if(anim->frames_size == 0) return true;
      
      //dealloc every frames' pixel buffer
      for(uint32_t i = 0; i < anim->frames_size; i++)
        if(anim->frames[i].pixels != NULL) free(anim->frames[i].pixels);

      //dealloc frame array
      free(anim->frames);

      //zero the rest
      memset(anim, 0, sizeof(animation_s));
      return true;
    }    
  }
}