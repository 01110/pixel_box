#include <cstring>
#include <cinttypes>
#include <cstdlib>
#include <cmath>

#define INDEX_STREAM_ALLOCATION_BLOCK 128

namespace img_parse
{
  typedef enum error_code_e
  {
    error_code_ok = 0,
    error_code_mem_alloc = 1,
    error_code_not_supported = 2,
    error_code_null_pt = 3,
    error_code_out_of_bounds = 4,
    error_code_parsed = 5,
    error_code_inconsistence = 6,
  } error_code_e;

  typedef enum block_type_e
  {
    block_type_image_descriptor = 0x2C,
    block_type_trailer = 0x3B,
    block_type_extension_introducer = 0x21,
  } block_type_e;

  typedef enum block_label_e
  {
    block_label_graphic_control = 0xF9,
    block_label_comment = 0xFE,
    block_label_plain_text = 0x01,
    block_label_application = 0xFF,
  } block_label_e;

  typedef struct color_s
  {
    uint8_t r;
    uint8_t g;
    uint8_t b;
  } color_s;

  typedef struct lsd_fields_s
  {
    uint8_t global_color_table_size :3;
    uint8_t sort_flag :1;
    uint8_t color_resolution :3;
    uint8_t global_color_table_flag :1;
  } lsd_fields_s;

  typedef struct
  {
    uint8_t local_color_table_size :3;
    uint8_t reserved :2;
    uint8_t sort_flag :1;
    uint8_t interlace_flag :1;
    uint8_t local_color_table_flag :1;
  } id_fields_s;

  typedef struct
  {
    uint8_t disposal_method:3;
    uint8_t user_input_flag:1;
    uint8_t transparent_color_flag:1;
  } gce_fields_s;  
  
  typedef struct logical_screen_descriptor_s
  {
    uint16_t width;
    uint16_t height;
    lsd_fields_s fields;
    uint8_t  background_color_index;
    uint8_t  pixel_aspect_ratio;
  } logical_screen_descriptor_s;

  typedef struct graphic_control_extension_s
  {
    gce_fields_s fields;
    uint16_t delay_time_10ms;
    uint8_t  transparent_color_index;
    bool valid;
  } graphic_control_extension_s;

  typedef struct image_descriptor_s
  {
    uint16_t left_position;
    uint16_t top_position;
    uint16_t width;
    uint16_t height;
    id_fields_s fields;
  } image_descriptor_s;

  typedef struct code_table_entry_s
  {
    uint8_t* string;
    uint8_t string_size;
  } code_table_entry_s;

  typedef struct code_table_s
  {
    code_table_entry_s* entries;
    uint32_t entries_count;
    uint16_t cc;
    uint16_t eoi;
  } code_table_s;

  typedef struct image_s
  {
    image_descriptor_s id;

    //local color table
    color_s* lct;
    uint32_t lct_size;

    //graphic control extension
    graphic_control_extension_s gce;

    //lzw parse
    uint8_t starting_code_size;
    uint8_t code_size;

    uint8_t* lzw; //lzw compressed image data after concatenating from image data blocks
    uint32_t lzw_size;
    uint32_t lzw_offset_byte;
    uint8_t  lzw_offset_bit;

    code_table_s code_table;

    uint8_t* index_stream;  //output pixels pointing to color table indexes
    uint32_t index_stream_size;
    uint32_t index_stream_offset;

    uint32_t last_code; //code parsed in the previous step
    uint32_t code; //currently parsed code

    //output data in RGB similar to FastLED
    color_s* output;  
    uint32_t output_size;

  } image_s;  

  typedef struct gif_parse_context_s
  {
    bool parsed;

    //raw input data to parse
    uint8_t* input;
    uint32_t input_size;
    uint32_t offset;

    logical_screen_descriptor_s lsd;

    //global color table
    color_s *gct;
    uint32_t gct_size;

    //last graphic control extension
    graphic_control_extension_s last_gce;

    //dynamically allocated array for storing frames/images
    image_s* images;
    uint32_t images_size;

  } gif_parse_context_s;

  error_code_e init(gif_parse_context_s& ctx, uint8_t* input, uint32_t input_size);
  error_code_e parse(gif_parse_context_s& ctx);
  void deinit(gif_parse_context_s& ctx);  
}
