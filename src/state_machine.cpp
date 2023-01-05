#include "state_machine.hpp"

#include <FastLED.h>

#include "web.hpp"
#include "gif_parse.hpp"
#include "png_parse.hpp"

namespace pixelbox
{
  namespace state_machine
  {
    extern CRGB connecting_image[];        //image displayed on startup/during connecting to Wi-Fi
    pixelbox::anim::animation_s animation; //animation data, for displaying GIF files

    void click_cb() //on click let's display the next stored image from flash
    { 
      String act;
      pixelbox::web::get_displayed_image(act);
      pixelbox::web::select_next_image(act);
    }

    void image_updated() //on image updated try to parse and display image
    {
      //read the displayed image's name and open it
      String filename;
      if(!pixelbox::web::get_displayed_image(filename)) return;
      File image_file = LittleFS.open("/images/" + filename, "r");
      if(!image_file) return;

      //currently only supports PNG and GIF, check the file extension
      bool png = true;
      if(filename.endsWith(".png")) png = true;
      else if(filename.endsWith(".gif")) png = false;
      else return;

      //read image file into RAM
      uint32_t img_size = image_file.size();
      uint8_t* img_buf = (uint8_t*) malloc(img_size);
      if(img_buf == NULL) return;
      if((size_t)image_file.read((uint8_t*)img_buf, img_size) != img_size)
      {
        free(img_buf);
        image_file.close();
        return;
      }
      image_file.close(); //we don't need the file to be open any more, close it

      //temporary buffer for image data to be displayed
      CRGB image[WS_LED_NUM];

      if(png)
      {
        //init the PNG parsing context
        img_parse::png_parse_context_s ctx;
        if(!img_parse::init(ctx, img_buf, img_size)) return;
        free(img_buf); //after init, image data copied into the ctx, no need for the img_buf (can be optimized if we read the image data from flash right into the context)

        //parse and check for error OR image with invalid size
        if(!img_parse::parse(ctx) || (ctx.hdr.height != 8 || ctx.hdr.width != 8))
        {
          img_parse::deinit(ctx);
          pixelbox::ws2812b_8x8::set_color(CRGB::Red); //display red color for error
          return;
        }

        //export output pixel data from the context into the CRGB array to pass it to fastled
        if(ctx.pixel_size == 3)
          memcpy(image, ctx.unfiltered_data, sizeof(image));
        else if(ctx.pixel_size == 4)
        {
          for(u8 i = 0; i < WS_LED_NUM; i++)
          {
            image[i].r = ctx.unfiltered_data[4*i+0];
            image[i].g = ctx.unfiltered_data[4*i+1];
            image[i].b = ctx.unfiltered_data[4*i+2];
          }
        }

        //dealloc everything left from the parsing
        img_parse::deinit(ctx);

        //set the image to be displayed
        pixelbox::ws2812b_8x8::set(image);
      }
      else
      {
        //init the GIF parsing context
        img_parse::gif_parse_context_s ctx;        
        if(img_parse::init(ctx, img_buf, img_size) != img_parse::error_code_ok) return;
        free(img_buf); //after init, image data copied into the ctx, no need for the img_buf (can be optimized if we read the image data from flash right into the context)
        
        //parse and check for error OR image with invalid size
        if(img_parse::parse(ctx) != img_parse::error_code_ok || (ctx.lsd.height != 8 || ctx.lsd.width != 8))
        {
          img_parse::deinit(ctx);
          pixelbox::ws2812b_8x8::set_color(CRGB::Red); //display red color for error
          return;
        }

        if(ctx.images_size == 1) //if it's an image, simply set it
        {
          memcpy(image, ctx.images[0].output, sizeof(image));
          pixelbox::ws2812b_8x8::set(image);
        }
        else //if it's an animation export the frames into an animation and set it
        {
          pixelbox::anim::animation_init(&animation); //dealloc if necessary and zero everything
          for(uint32_t i = 0; i < ctx.images_size; i++)
          {
            if(!ctx.images[i].gce.valid) continue; //skip frames without gce
            pixelbox::anim::add_frame(&animation, 
                            ctx.images[i].gce.delay_time_10ms * 10, 
                            ctx.images[i].id.left_position, 
                            ctx.images[i].id.top_position,
                            (CRGB*)ctx.images[i].output,
                            ctx.images[i].output_size);
          }
          pixelbox::ws2812b_8x8::set(&animation);
        }

        //dealloc everything left from the parsing
        img_parse::deinit(ctx);
      }
    }

    void load_brightness()
    {
      File f = LittleFS.open("/brightness", "r");
      if(!f)
      {
        File w = LittleFS.open("/brightness", "w");
        w.write("50");
        w.close();
        pixelbox::ws2812b_8x8::set_brightness_percent(50);
        return;
      }
      pixelbox::ws2812b_8x8::set_brightness_percent(f.readString().toInt());
      f.close();
    }

    void load_max_current()
    {
      File f = LittleFS.open("/max_current", "r");
      if(!f)
      {
        File w = LittleFS.open("/max_current", "w");
        w.write("1500");
        w.close();
        pixelbox::ws2812b_8x8::set_max_current(1500);
        return;
      }
      pixelbox::ws2812b_8x8::set_max_current(f.readString().toInt());
      f.close();
    }

    void setup()
    {
      //on startup set the connecting image if no image is uploaded/storage is empty
      pixelbox::ws2812b_8x8::set(connecting_image);
      image_updated();
      load_max_current();
      load_brightness();
    }
  }  
}
