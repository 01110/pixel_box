#include <tinf.h>
#include <cstring>
#include <cinttypes>
#include <cstdlib>
#include <cmath>

//Materials used for writing this parser:

//PNG format standard, RFC 2083: https://www.rfc-editor.org/rfc/rfc2083
//libPNG (ideas), generic and mature PNG parsing lib: http://libpng.org/pub/png/libpng.html
//uPNG (ideas), a very small PNG parsing lib: https://github.com/elanthis/upng
//tinf (used as third party code), a very tiny implementation of the inflate algo: https://github.com/jibsen/tinf

namespace img_parse
{
  //PNG chunk types
  typedef enum chunk_type_e
  {
    chunk_type_inv  = 0x58585858, //invalid
    chunk_type_ihdr = 0x49484452,
    chunk_type_idat = 0x49444154,
    chunk_type_iend = 0x49454E44,
  } chunk_type_e;

  //PNG filter types
  typedef enum filter_method_e
  {
    filter_method_none = 0,
    filter_method_sub = 1,
    filter_method_up = 2,
    filter_method_avg = 3,
    filter_method_paeth = 4,
  } filter_method_e;

  //PNG header
  typedef struct ihdr_s
  {
    uint32_t width;
    uint32_t height;
    uint8_t  bit_depth;
    uint8_t  color_type;
    uint8_t  compression_method;
    uint8_t  filter_method;
    uint8_t  interlace_method;
  } ihdr_s;

  //generic PNG chunk
  typedef struct chunk_data_s
  {
    uint32_t len;
    uint32_t type;
    uint32_t crc32;
  } chunk_data_s;
  
  typedef struct png_parse_context_s
  {
    ihdr_s hdr; //parsed header of the PNG file
    uint8_t pixel_size;
    bool parsed; //file is parsed and unfiltered data/size are valid

    //raw PNG data to parse
    uint8_t* data;
    size_t size;
    uint32_t offset;

    //uncompressed idata data
    uint8_t* inflated_data;
    unsigned int inflated_size;

    //completely reconstructed image
    uint32_t scanline_index;
    uint32_t stride;    
    uint8_t* unfiltered_data;  
    uint32_t unfiltered_size;
  } png_parse_context_s;

  bool init(png_parse_context_s& ctx, uint8_t* data, uint32_t len);
  void deinit(png_parse_context_s& ctx);
  bool parse(png_parse_context_s& ctx);
}
