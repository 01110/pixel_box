#include "gif_parse.hpp"

#include <cstring>
#include <cinttypes>
#include <cstdlib>
#include <cmath>

namespace img_parse
{
  error_code_e parse_gct(gif_parse_context_s& ctx)
  {
    if(ctx.parsed) return error_code_parsed;
    if(!ctx.lsd.fields.global_color_table_flag) return error_code_ok; //if there is no gct we are done
    if(ctx.offset != 6 + 7) return error_code_out_of_bounds; //should follow header and lsd

    //calc the gct size and check the input size
    uint32_t gct_size = (0x01 << (ctx.lsd.fields.global_color_table_size + 1)) * 3;    
    if(ctx.input_size < (6 + 7 + gct_size)) return error_code_inconsistence;

    //allocate memory for gct and copy it
    ctx.gct = (color_s*)calloc(gct_size, 1);
    if(ctx.gct == NULL) return error_code_null_pt;
    ctx.gct_size = gct_size / 3;
    memcpy(ctx.gct, ctx.input + ctx.offset, gct_size);

    ctx.offset += gct_size;
    return error_code_ok;
  }

  error_code_e parse_lsd(gif_parse_context_s& ctx)
  {
    if(ctx.parsed) return error_code_parsed;
    if(ctx.offset != 6) return error_code_inconsistence; //should follow header
    if(ctx.input_size < 6 + 6) return error_code_out_of_bounds;

    memcpy(&ctx.lsd.width, ctx.input + ctx.offset, sizeof(ctx.lsd.width));
    memcpy(&ctx.lsd.height, ctx.input + ctx.offset + 2, sizeof(ctx.lsd.height));
    memcpy(&ctx.lsd.fields, ctx.input + ctx.offset + 4, sizeof(ctx.lsd.fields));
    ctx.lsd.background_color_index = ctx.input[ctx.offset + 5];
    ctx.lsd.pixel_aspect_ratio = ctx.input[ctx.offset + 6];

    ctx.offset += 7;
    return error_code_ok;
  }

  error_code_e check_header(gif_parse_context_s& ctx)
  {
    if(ctx.input_size < 6) return error_code_inconsistence;
    if(ctx.offset != 0) return error_code_inconsistence;

    //ASCII 'GIF'
    if(ctx.input[0] != 0x47) return error_code_inconsistence;
    if(ctx.input[1] != 0x49) return error_code_inconsistence;
    if(ctx.input[2] != 0x46) return error_code_inconsistence;

    //version '89a' and '87a'
    if(ctx.input[3] != 0x38) return error_code_inconsistence;
    if(ctx.input[4] != 0x39 && ctx.input[4] != 0x37) return error_code_inconsistence;
    if(ctx.input[5] != 0x61) return error_code_inconsistence;

    ctx.offset += 6;
    return error_code_ok;
  }
  
  error_code_e add_code_table_entry(image_s* image, uint16_t code, void* string_pt, uint32_t string_size)
  {
    if(!image) return error_code_null_pt;
    if(image->code_size < 2) return error_code_inconsistence;
    if(!image->code_table.entries) return error_code_null_pt;
    if(code >= 0x01 << (image->code_size + 1)) return error_code_out_of_bounds; //over indexing

    //alloc and copy

    //if the allocation would be too small for (32bit) allocate bigger space
    uint32_t alloc_size = string_size;
    if(alloc_size < 4) alloc_size = 4;

    image->code_table.entries[code].string = (uint8_t*)calloc(0x01, alloc_size);
    if(!image->code_table.entries[code].string) return error_code_mem_alloc;
    memcpy(image->code_table.entries[code].string, string_pt, string_size);
    image->code_table.entries_count++;
    image->code_table.entries[code].string_size = string_size;

    if(image->code_table.entries_count == (uint32_t)(0x01 << (image->code_size + 1))) //code size bump time baby
    {
      image->code_size++;
      image->code_table.entries = (code_table_entry_s*)realloc(image->code_table.entries, sizeof(code_table_entry_s) * (0x01 << (image->code_size + 1)));
      if(!image->code_table.entries) return error_code_mem_alloc;
      //zero the newly allocated space
      memset(image->code_table.entries + (0x1 << image->code_size), 0, sizeof(code_table_entry_s) * (0x1 << image->code_size));
    }

    return error_code_ok;
  }

  error_code_e init_code_table(image_s* image)
  {
    if(!image) return error_code_null_pt;
    if(image->code_size < 2) return error_code_inconsistence; //min allowed code size

    //allocate mem for code table                     
    image->code_table.entries = (code_table_entry_s*)calloc(0x01 << (image->code_size + 1), sizeof(code_table_entry_s));
    image->code_table.entries_count = 0;
    if(!image->code_table.entries) return error_code_mem_alloc;

    //add trivial codes
    for(uint16_t code = 0; code < (0x01 << image->code_size) + 2; code++)
      add_code_table_entry(image, code, &code, 0x01); //+2 means it adds cc and eoi too

    //control codes for convenience
    image->code_table.cc =  (0x01 << image->code_size);
    image->code_table.eoi = (0x01 << image->code_size) + 1;

    return error_code_ok;
  }

  error_code_e deinit_code_table(image_s* image)
  {
    if(!image) return error_code_null_pt;

    //free the mem of entries
    for(uint16_t code = 0; code < image->code_table.entries_count; code++)
    {
      if(image->code_table.entries[code].string)
      {
        free(image->code_table.entries[code].string);
        image->code_table.entries[code].string = NULL;
        image->code_table.entries[code].string_size = 0;
      }
    }

    //free the table
    if(image->code_table.entries)
    {
      free(image->code_table.entries);
      image->code_table.entries = NULL;
      image->code_table.entries_count = 0;
      image->code_table.cc = 0;
      image->code_table.eoi = 0;
    }
    return error_code_ok;
  }

  error_code_e parse_lct(gif_parse_context_s& ctx)
  {
    if(ctx.parsed) return error_code_parsed;
    if(ctx.images == NULL) return error_code_null_pt;
    if(ctx.images_size == 0) return error_code_inconsistence;

    image_s* image_pt = ctx.images + (ctx.images_size - 1);
    if(!image_pt->id.fields.local_color_table_flag) return error_code_ok;

    //calc the lct size and check the input size
    uint32_t lct_size = (0x01 << (image_pt->id.fields.local_color_table_size + 1)) * 3;
    if(ctx.input_size < (ctx.offset + lct_size)) return error_code_out_of_bounds;

    //allocate memory for lct and copy it
    image_pt->lct = (color_s*)calloc(lct_size, 1);
    if(image_pt->lct == NULL) return error_code_mem_alloc;
    image_pt->lct_size = lct_size / 3;
    memcpy(image_pt->lct, ctx.input + ctx.offset, lct_size);

    ctx.offset += lct_size;
    return error_code_ok;
  }

  error_code_e parse_gce(gif_parse_context_s& ctx)
  {
    if(ctx.parsed) return error_code_parsed;
    if(ctx.input_size < (ctx.offset + 8)) return error_code_out_of_bounds;
    if(*(ctx.input + ctx.offset) != block_type_extension_introducer) return error_code_inconsistence;
    if(*(ctx.input + ctx.offset + 1) != block_label_graphic_control) return error_code_inconsistence;

    //checks of fixed length block
    if(*(ctx.input + ctx.offset + 2) != 4) return error_code_inconsistence; //sub block data length
    if(*(ctx.input + ctx.offset + 7) != 0) return error_code_inconsistence; //block terminator

    memcpy(&ctx.last_gce.fields, ctx.input + ctx.offset + 3, 1);
    memcpy(&ctx.last_gce.delay_time_10ms, ctx.input + ctx.offset + 4, 2);
    memcpy(&ctx.last_gce.transparent_color_index, ctx.input + ctx.offset + 6, 1);
    ctx.last_gce.valid = true;

    ctx.offset += 8;
    return error_code_ok;
  }

  error_code_e read_code(image_s* image)
  {
    if(image == NULL) return error_code_null_pt;
    if(image->lzw_offset_byte >= image->lzw_size) return error_code_out_of_bounds;

    //save the last code
    image->last_code = image->code;

    image->code = 0;
    //over reading protection for valgrind
    uint32_t read_size = (image->lzw_size - image->lzw_offset_byte) > 4 ? 4 : image->lzw_size - image->lzw_offset_byte;    

    //read two bytes
    memcpy(&(image->code), image->lzw + image->lzw_offset_byte, read_size);

    //remove the unnecessary bits before the code
    image->code = image->code >> image->lzw_offset_bit; 

    //maks the unnecessary bits after the code
    uint16_t mask = 0;
    for(uint16_t i = 0; i < (image->code_size + 1); i++)
      mask |= 0x01 << i;
    image->code &= mask;

    //adjust the offsets
    image->lzw_offset_bit += image->code_size + 1;
    if(image->lzw_offset_bit >= 8)
    {
      image->lzw_offset_byte += image->lzw_offset_bit / 8;
      image->lzw_offset_bit = image->lzw_offset_bit % 8;
    }

    return error_code_ok;
  }

  bool is_in_code_table(image_s* image, uint16_t code)
  {
    if(code > (0x01 << (image->code_size + 1))) return false;
    return image->code_table.entries[code].string_size != 0 ? true : false;
  }

  error_code_e output_index(image_s* image, bool first_only = false, bool last = false)
  {
    if(!image) return error_code_null_pt;
    uint32_t code = last ? image->last_code : image->code;
    if(!is_in_code_table(image, code)) return error_code_inconsistence;
    uint32_t copy_size = first_only ? 1 : image->code_table.entries[code].string_size;

    //realloc if necessary
    if(image->index_stream_offset + copy_size > image->index_stream_size)
    {
      image->index_stream = (uint8_t*)realloc(image->index_stream, image->index_stream_size + INDEX_STREAM_ALLOCATION_BLOCK);
      if(!image->index_stream) return error_code_mem_alloc;
      image->index_stream_size = image->index_stream_size + INDEX_STREAM_ALLOCATION_BLOCK;
    }

    //append string to the index stream
    memcpy(image->index_stream + image->index_stream_offset, image->code_table.entries[code].string, copy_size);
    image->index_stream_offset += copy_size;

    return error_code_ok;
  }

  error_code_e free_image_parsing_memory(image_s* image)
  {
    if(!image) return error_code_null_pt;

    //free local color table
    if(image->lct)
    {
      free(image->lct);
      image->lct = NULL;
      image->lct_size = 0;
    }

    //free LZW compressed data
    if(image->lzw)
    {
      free(image->lzw);
      image->lzw = NULL;
      image->lzw_size = 0;
      image->lzw_offset_bit = 0;
      image->lzw_offset_byte = 0;
    }

    //free the entire code table
    deinit_code_table(image);

    if(image->index_stream)
    {
      free(image->index_stream);
      image->index_stream = NULL;
      image->index_stream_offset = 0,
      image->index_stream_size = 0;
    }

    return error_code_ok;
  }

  error_code_e deinit_image(image_s* image)
  {
    if(!image) return error_code_null_pt;

    free_image_parsing_memory(image);
    if(image->output)
    {
      free(image->output);
      image->output = NULL;
      image->output_size = 0;
    }

    return error_code_ok;
  }

  error_code_e parse_image(gif_parse_context_s& ctx)
  {    
    if(ctx.parsed) return error_code_parsed;
    if(*(ctx.input + ctx.offset) != block_type_image_descriptor) return error_code_inconsistence;
    if(ctx.input_size - ctx.offset < 10) return error_code_inconsistence; //at least the idesc

    //allocate mem for the new image data
    if(ctx.images == NULL)
    {
      ctx.images = (image_s*)calloc(1, sizeof(image_s));
      if(ctx.images == NULL) return error_code_mem_alloc;
      ctx.images_size = 1;
    }
    else
    {
      ctx.images = (image_s*)realloc(ctx.images, sizeof(image_s) * (ctx.images_size + 1));
      if(ctx.images == NULL) return error_code_mem_alloc;
      //init the newly allocated image struct
      memset(ctx.images + ctx.images_size, 0, sizeof(image_s));
      ctx.images_size++;
    }

    image_s* image_pt = ctx.images + (ctx.images_size - 1);

    //parse image descriptor of the image
    memcpy(&image_pt->id.left_position, ctx.input + ctx.offset + 1, 2);
    memcpy(&image_pt->id.top_position, ctx.input + ctx.offset + 1 + 2, 2);
    memcpy(&image_pt->id.width, ctx.input + ctx.offset + 1 + 4, 2);
    memcpy(&image_pt->id.height, ctx.input + ctx.offset + 1 + 6, 2);
    memcpy(&image_pt->id.fields, ctx.input + ctx.offset + 1 + 8, 1);

    //don't supported functions
    if(image_pt->id.fields.interlace_flag) return error_code_not_supported;
    if(image_pt->id.height != ctx.lsd.height) return error_code_not_supported;
    if(image_pt->id.width != ctx.lsd.width) return error_code_not_supported;

    if(ctx.last_gce.valid)
    {
      memcpy(&image_pt->gce, &ctx.last_gce, sizeof(graphic_control_extension_s));
      ctx.last_gce.valid = false;
    }

    //parse local color table
    ctx.offset += 1 + 9;
    error_code_e err = parse_lct(ctx);
    if(err != error_code_ok) return err;

    //minimum code size in bits
    image_pt->code_size = *(ctx.input + ctx.offset); 
    if(image_pt->code_size < 2) return error_code_inconsistence; //min allowed code size
    image_pt->starting_code_size = image_pt->code_size;

    //read and concatenate data sub-blocks into lzw buffer
    uint32_t offset = ctx.offset + 1;
    while(*(ctx.input + offset) != 0x00) //block terminator
    {
      uint8_t sub_block_size = *(ctx.input + offset);
      if((ctx.input_size - offset) < ((uint32_t)sub_block_size + 1)) return error_code_out_of_bounds;      
      image_pt->lzw = (uint8_t*)realloc(image_pt->lzw, image_pt->lzw_size + sub_block_size);
      if(image_pt->lzw == NULL) return error_code_mem_alloc;
      memcpy(image_pt->lzw + image_pt->lzw_size, ctx.input + offset + 1, sub_block_size);
      image_pt->lzw_size += sub_block_size;
      image_pt->lzw_offset_byte = 0;
      image_pt->lzw_offset_bit = 0;
      offset += sub_block_size + 1;
    }
    ctx.offset = offset + 1;

    //parse lzw: http://giflib.sourceforge.net/whatsinagif/lzw_image_data.html
    
    err = init_code_table(image_pt);
    if(err != error_code_ok) return err;

    //first code should be cc
    err = read_code(image_pt);
    if(err != error_code_ok) return err;
    if(image_pt->code != image_pt->code_table.cc) return error_code_inconsistence;

    //second code should be in the table, output it right away
    err = read_code(image_pt);
    if(err != error_code_ok) return err;
    err = output_index(image_pt);
    if(err != error_code_ok) return err;

    for(uint32_t i = 0;; i++)
    {
      err = read_code(image_pt); //over inedxing protection included
      if(err != error_code_ok) return err;
      if(image_pt->code == image_pt->code_table.cc)
      {
        image_pt->code_size = image_pt->starting_code_size;
        deinit_code_table(image_pt);
        init_code_table(image_pt);
        break;
      }
      if(image_pt->code == image_pt->code_table.eoi) break; //successfully parsed the entire lzw data array
      if(is_in_code_table(image_pt, image_pt->code))
      {
        err = output_index(image_pt);
        if(err != error_code_ok) return err;
        uint8_t c = *(image_pt->code_table.entries[image_pt->code].string); //first index of the current code
        //overcopy for fewer allocation
        err = add_code_table_entry(image_pt, image_pt->code_table.entries_count, image_pt->code_table.entries[image_pt->last_code].string, image_pt->code_table.entries[image_pt->last_code].string_size + 1); 
        if(err != error_code_ok) return err;
        image_pt->code_table.entries[image_pt->code_table.entries_count - 1].string[image_pt->code_table.entries[image_pt->code_table.entries_count - 1].string_size - 1] = c;
      }
      else
      {
        err = output_index(image_pt, false, true); //all indexes of the last code
        if(err != error_code_ok) return err;
        err = output_index(image_pt, true, true); //first index of the last code
        if(err != error_code_ok) return err;
        uint32_t last_string_size = image_pt->code_table.entries[image_pt->last_code].string_size;
        err = add_code_table_entry(image_pt, image_pt->code, image_pt->index_stream + image_pt->index_stream_offset - last_string_size - 1, last_string_size + 1);
        if(err != error_code_ok) return err;
      }
    }

    deinit_code_table(image_pt);
    if(image_pt->lzw)
    {
      free(image_pt->lzw);
      image_pt->lzw = NULL;
      image_pt->lzw_size = 0;
      image_pt->lzw_offset_bit = 0;
      image_pt->lzw_offset_byte = 0;
    }

    //just some extra checking
    if(image_pt->index_stream_offset != (image_pt->id.height * image_pt->id.width))
      return error_code_inconsistence;

    //create RGBA array based on index stream and color tables
    color_s* color_table = image_pt->id.fields.local_color_table_flag ? image_pt->lct : ctx.gct;
    uint32_t color_table_size = image_pt->id.fields.local_color_table_flag ? image_pt->lct_size : ctx.gct_size;
    image_pt->output = (color_s*)calloc(image_pt->index_stream_offset, sizeof(color_s));
    if(!image_pt->output) return error_code_mem_alloc;
    image_pt->output_size = image_pt->index_stream_offset;
    for(uint32_t i = 0; i < image_pt->index_stream_offset; i++)
    {
      if(image_pt->index_stream[i] > color_table_size) return error_code_out_of_bounds; //protection against over indexing color_table
      memcpy(&image_pt->output[i], &color_table[image_pt->index_stream[i]], sizeof(color_s));
    }

    return error_code_ok;
  }

  error_code_e parse_extension(gif_parse_context_s& ctx)
  {
    if(ctx.parsed) return error_code_parsed;
    if(ctx.input_size < ctx.offset + 4) return error_code_out_of_bounds;
    if(*(ctx.input + ctx.offset) != block_type_extension_introducer) return error_code_inconsistence;

    //supported extensions
    if(*(ctx.input + ctx.offset + 1) == block_label_graphic_control) return parse_gce(ctx);

    //skipping all the blocks of the extension
    uint8_t block_size = *(ctx.input + ctx.offset + 2);
    if((ctx.input_size - ctx.offset) < (uint32_t)(3 + block_size)) return error_code_out_of_bounds;

    uint32_t offset = ctx.offset + 3 + block_size;    
    while(*(ctx.input + offset) != 0x00) //block terminator
    {
      if((ctx.input_size - offset) < *(ctx.input + offset)) return error_code_out_of_bounds;
      offset += *(ctx.input + offset) + 1;
    }

    ctx.offset = offset + 1;
    return error_code_ok;
  }

  error_code_e parse_next_block(gif_parse_context_s& ctx)
  {
    if(ctx.parsed) return error_code_parsed;

    uint8_t block_label = *(ctx.input + ctx.offset);

    switch (block_label)
    {
    case block_type_image_descriptor:
    {
      uint32_t images_size = ctx.images_size;
      error_code_e err =parse_image(ctx);
      if(err != error_code_ok) return err;
      if(ctx.images_size == images_size + 1) //if there is a new images after parse, clean up the last parsed one
        free_image_parsing_memory(&ctx.images[images_size]);
      break;
    }
    case block_type_trailer:
    {
      ctx.parsed = true;
      break;
    }
    case block_type_extension_introducer:
    {
      //skips all extension blocks except gce
      parse_extension(ctx);
      break;
    }
    default:
      //handle unhandled blocks :)
      break;
    }

    return error_code_ok;
  }

  error_code_e init(gif_parse_context_s& ctx, uint8_t* input, uint32_t input_size)
  {
    //init the context struct
    memset(&ctx, 0, sizeof(ctx));

    //dynamically allocate mem for input and store the input data
    ctx.input = (uint8_t*)calloc(input_size, 1);
    if(ctx.input == NULL) return error_code_mem_alloc;
    ctx.input_size = input_size;
    memcpy(ctx.input, input, ctx.input_size);

    return error_code_ok;
  }

  error_code_e parse(gif_parse_context_s& ctx)
  {
    if(ctx.parsed) return error_code_parsed;
    error_code_e err;
    err = check_header(ctx);
    if(err != error_code_ok) return err;
    err = parse_lsd(ctx);
    if(err != error_code_ok) return err;
    err = parse_gct(ctx);
    if(err != error_code_ok) return err;

    while(!ctx.parsed)
    {
      err = parse_next_block(ctx);
      if(err != error_code_ok) return err;
    }

    free(ctx.input);
    ctx.input = NULL;
    ctx.input_size = 0;
    return error_code_ok;
  }

  void deinit(gif_parse_context_s& ctx)
  {
    //deallocate all dynamically allocated memory and zero the entire struct
    if(ctx.input) free(ctx.input);
    if(ctx.gct) free(ctx.gct);
    if(ctx.images)
    {
      for(uint32_t i = 0; i < ctx.images_size; i++) deinit_image(&ctx.images[i]);
      free(ctx.images);
    }
    memset(&ctx, 0, sizeof(ctx));
  }  
}
