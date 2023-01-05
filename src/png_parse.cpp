#include "png_parse.hpp"

#include <tinf.h>
#include <cstring>
#include <cinttypes>
#include <cstdlib>
#include <cmath>

namespace img_parse
{
  //PNG byte order to host
  uint32_t read_u32(png_parse_context_s& ctx, uint64_t offset)
  {    
    return (uint32_t)((uint32_t)(ctx.data[ctx.offset + offset] << 24) + (uint32_t)(ctx.data[ctx.offset + offset + 1] << 16) + (uint32_t)(ctx.data[ctx.offset + offset + 2] << 8) + (uint32_t)(ctx.data[ctx.offset + offset + 3] << 0)); 
  }

  //PNG byte order to host
  uint8_t read_u8(png_parse_context_s& ctx, uint64_t offset)
  {
    return *(ctx.data + ctx.offset + offset);
  }

  bool check_next_chunk(png_parse_context_s& ctx, chunk_data_s& cd)
  {
    //first must be the file header
    if(ctx.offset < 8) return false;

    //if remaining bytes not enough for the smallest chunk possible, it's invalid
    uint32_t remaining = ctx.size - ctx.offset;
    if(remaining < 12) return false;    

    //if the chunk would be bigger than the remaining bytes, it's invalid
    cd.len = read_u32(ctx, 0);
    if(remaining < (cd.len + 4 + 4)) return false;

    //check the integrity of the chunk with crc32
    cd.crc32 = read_u32(ctx, cd.len + 4 + 4);
    uint32_t crc_calc = tinf_crc32(ctx.data + ctx.offset + 4, cd.len + 4);
    if(cd.crc32 != crc_calc) return false;

    //read the chunk type
    cd.type = read_u32(ctx, 4);
    return true;
  }

  bool parse_ihdr(png_parse_context_s& ctx)
  {
    chunk_data_s cd;
    if(!check_next_chunk(ctx, cd)) return false; //does crc check too
    if(cd.type != chunk_type_ihdr) return false;
    if(cd.len != 13) return false; //IHDR must be 13 bytes long
    ctx.offset += 4 + 4; //len, type

    ctx.hdr.width = read_u32(ctx, 0);
    ctx.hdr.height = read_u32(ctx, 4);
    ctx.hdr.bit_depth = read_u8(ctx, 8);
    ctx.hdr.color_type = read_u8(ctx, 9);
    ctx.hdr.compression_method = read_u8(ctx, 10);
    ctx.hdr.filter_method = read_u8(ctx, 11);
    ctx.hdr.interlace_method = read_u8(ctx, 12);

    ctx.offset += cd.len + 4; //data + crc32
    return true;
  }

  uint8_t paeth_predictor(uint8_t a, uint8_t b, uint8_t c)
  {
    // see RFC @ 6.6 below
    
    int p = a + b - c;
    int pa = abs(p - a);
    int pb = abs(p - b);
    int pc = abs(p - c);
    int pr;
    if(pa <= pb && pa <= pc)
        pr = a;
    else if(pb <= pc)
        pr = b;
    else
        pr = c;
    return pr;
  }

  void unfilter_scanline(png_parse_context_s& ctx)
  {
    //PNG images can be filtered: https://www.rfc-editor.org/rfc/rfc2083#page-31
    //in order to reconstruct the image, we need to unfilter scanlines (rows) of the image

    uint8_t filter_method = *(ctx.inflated_data + (ctx.stride + 1) * ctx.scanline_index);
    switch (filter_method)
    {
    case filter_method_none:
    {
      memcpy(ctx.unfiltered_data + ctx.stride * ctx.scanline_index, 
             ctx.inflated_data + (ctx.stride + 1) * ctx.scanline_index + 1,
             ctx.stride);
      break;
    }
    case filter_method_sub:
    {
      // RFC @ 6.3:
      // The Sub filter transmits the difference between each byte and the
      // value of the corresponding byte of the prior pixel.
      // To compute the Sub filter, apply the following formula to each
      // byte of the scanline:
      //    Sub(x) = Raw(x) - Raw(x-bpp)
      // where x ranges from zero to the number of bytes representing the
      // scanline minus one, Raw(x) refers to the raw data byte at that
      // byte position in the scanline, and bpp is defined as the number of
      // bytes per complete pixel, rounding up to one. For example, for
      // color type 2 with a bit depth of 16, bpp is equal to 6 (three
      // samples, two bytes per sample); for color type 0 with a bit depth
      // of 2, bpp is equal to 1 (rounding up); for color type 4 with a bit
      // depth of 16, bpp is equal to 4 (two-byte grayscale sample, plus
      // two-byte alpha sample).
      // Note this computation is done for each byte, regardless of bit
      // depth.  In a 16-bit image, each MSB is predicted from the
      // preceding MSB and each LSB from the preceding LSB, because of the
      // way that bpp is defined.
      // Unsigned arithmetic modulo 256 is used, so that both the inputs
      // and outputs fit into bytes.  The sequence of Sub values is
      // transmitted as the filtered scanline.
      // For all x < 0, assume Raw(x) = 0.
      // To reverse the effect of the Sub filter after decompression,
      // output the following value:
      //    Sub(x) + Raw(x-bpp)
      // (computed mod 256), where Raw refers to the bytes already decoded.

      for(uint32_t i = 0; i < ctx.stride; i++)
      {
        ctx.unfiltered_data[ctx.stride * ctx.scanline_index + i] = ctx.inflated_data[(ctx.stride + 1) * ctx.scanline_index + 1 + i];
        if(i < ctx.pixel_size)
          continue;
        else
          ctx.unfiltered_data[ctx.stride * ctx.scanline_index + i] += ctx.unfiltered_data[ctx.stride * ctx.scanline_index  + i - ctx.pixel_size];
      }
      break;
    }
    case filter_method_up:
    {
      // RFC @ 6.4:
      // The Up filter is just like the Sub filter except that the pixel
      // immediately above the current pixel, rather than just to its left,
      // is used as the predictor.
      // To compute the Up filter, apply the following formula to each byte
      // of the scanline:
      //    Up(x) = Raw(x) - Prior(x)
      // where x ranges from zero to the number of bytes representing the
      // scanline minus one, Raw(x) refers to the raw data byte at that
      // byte position in the scanline, and Prior(x) refers to the
      // unfiltered bytes of the prior scanline.
      // Note this is done for each byte, regardless of bit depth.
      // Unsigned arithmetic modulo 256 is used, so that both the inputs
      // and outputs fit into bytes.  The sequence of Up values is
      // transmitted as the filtered scanline.
      // On the first scanline of an image (or of a pass of an interlaced
      // image), assume Prior(x) = 0 for all x.
      // To reverse the effect of the Up filter after decompression, output
      // the following value:
      //    Up(x) + Prior(x)
      // (computed mod 256), where Prior refers to the decoded bytes of the
      // prior scanline.

      for(uint32_t i = 0; i < ctx.stride; i++)
      {
        ctx.unfiltered_data[ctx.stride * ctx.scanline_index + i] = ctx.inflated_data[(ctx.stride + 1) * ctx.scanline_index + 1 + i];
        if(ctx.scanline_index == 0)
          return;
        else
          ctx.unfiltered_data[ctx.stride * ctx.scanline_index + i] += ctx.unfiltered_data[ctx.stride * (ctx.scanline_index - 1) + i];
      }
      break;
    }
    case filter_method_avg:
    {
      // RFC @ 6.5:
      // The Average filter uses the average of the two neighboring pixels
      // (left and above) to predict the value of a pixel.
      // To compute the Average filter, apply the following formula to each
      // byte of the scanline:
      //    Average(x) = Raw(x) - floor((Raw(x-bpp)+Prior(x))/2)
      // where x ranges from zero to the number of bytes representing the
      // scanline minus one, Raw(x) refers to the raw data byte at that
      // byte position in the scanline, Prior(x) refers to the unfiltered
      // bytes of the prior scanline, and bpp is defined as for the Sub
      // filter.
      // Note this is done for each byte, regardless of bit depth.  The
      // sequence of Average values is transmitted as the filtered
      // scanline.
      // The subtraction of the predicted value from the raw byte must be
      // done modulo 256, so that both the inputs and outputs fit into
      // bytes.  However, the sum Raw(x-bpp)+Prior(x) must be formed
      // without overflow (using at least nine-bit arithmetic).  floor()
      // indicates that the result of the division is rounded to the next
      // lower integer if fractional; in other words, it is an integer
      // division or right shift operation.
      // For all x < 0, assume Raw(x) = 0.  On the first scanline of an
      // image (or of a pass of an interlaced image), assume Prior(x) = 0
      // for all x.
      // To reverse the effect of the Average filter after decompression,
      // output the following value:
      //    Average(x) + floor((Raw(x-bpp)+Prior(x))/2)
      // where the result is computed mod 256, but the prediction is
      // calculated in the same way as for encoding.  Raw refers to the
      // bytes already decoded, and Prior refers to the decoded bytes of
      // the prior scanline.

      for(uint32_t i = 0; i < ctx.stride; i++)
      {
        ctx.unfiltered_data[ctx.stride * ctx.scanline_index + i] = ctx.inflated_data[(ctx.stride + 1) * ctx.scanline_index + 1 + i];

        uint8_t r_a = 0;
        uint8_t r_b = 0;
        if(i >= ctx.pixel_size)
          r_a = ctx.unfiltered_data[ctx.stride * ctx.scanline_index  + i - ctx.pixel_size];
        if(ctx.scanline_index > 0)
          r_b = ctx.unfiltered_data[ctx.stride * (ctx.scanline_index - 1) + i];

        ctx.unfiltered_data[ctx.stride * ctx.scanline_index + i] += (r_a + r_b) / 2;
      }
      break;
    }
    case filter_method_paeth:
    {
      // RFC @ 6.6:
      // The Paeth filter computes a simple linear function of the three
      // neighboring pixels (left, above, upper left), then chooses as
      // predictor the neighboring pixel closest to the computed value.
      // This technique is due to Alan W. Paeth [PAETH].
      // To compute the Paeth filter, apply the following formula to each
      // byte of the scanline:
      //    Paeth(x) = Raw(x) - PaethPredictor(Raw(x-bpp), Prior(x),
      //                                       Prior(x-bpp))
      // where x ranges from zero to the number of bytes representing the
      // scanline minus one, Raw(x) refers to the raw data byte at that
      // byte position in the scanline, Prior(x) refers to the unfiltered
      // bytes of the prior scanline, and bpp is defined as for the Sub
      // filter.
      // Note this is done for each byte, regardless of bit depth.
      // Unsigned arithmetic modulo 256 is used, so that both the inputs
      // and outputs fit into bytes.  The sequence of Paeth values is
      // transmitted as the filtered scanline.
      // The PaethPredictor function is defined by the following
      // pseudocode:
      //    function PaethPredictor (a, b, c)
      //    begin
      //         ; a = left, b = above, c = upper left
      //         p := a + b - c        ; initial estimate
      //         pa := abs(p - a)      ; distances to a, b, c
      //         pb := abs(p - b)
      //         pc := abs(p - c)
      //         ; return nearest of a,b,c,
      //         ; breaking ties in order a,b,c.
      //         if pa <= pb AND pa <= pc then return a
      //         else if pb <= pc then return b
      //         else return c
      //    end
      // The calculations within the PaethPredictor function must be
      // performed exactly, without overflow.  Arithmetic modulo 256 is to
      // be used only for the final step of subtracting the function result
      // from the target byte value.
      // Note that the order in which ties are broken is critical and must
      // not be altered.  The tie break order is: pixel to the left, pixel
      // above, pixel to the upper left.  (This order differs from that
      // given in Paeth's article.)
      // For all x < 0, assume Raw(x) = 0 and Prior(x) = 0.  On the first
      // scanline of an image (or of a pass of an interlaced image), assume
      // Prior(x) = 0 for all x.
      // To reverse the effect of the Paeth filter after decompression,
      // output the following value:
      //    Paeth(x) + PaethPredictor(Raw(x-bpp), Prior(x), Prior(x-bpp))
      // (computed mod 256), where Raw and Prior refer to bytes already
      // decoded.  Exactly the same PaethPredictor function is used by both
      // encoder and decoder.

      for(uint32_t i = 0; i < ctx.stride; i++)
      {
        ctx.unfiltered_data[ctx.stride * ctx.scanline_index + i] = ctx.inflated_data[(ctx.stride + 1) * ctx.scanline_index + 1 + i];

        uint8_t r_a = 0;
        uint8_t r_b = 0;
        uint8_t r_c = 0;
        if(i >= ctx.pixel_size)
          r_a = ctx.unfiltered_data[ctx.stride * ctx.scanline_index  + i - ctx.pixel_size];
        if(ctx.scanline_index > 0)
          r_b = ctx.unfiltered_data[ctx.stride * (ctx.scanline_index - 1) + i];
        if(i >= ctx.pixel_size && ctx.scanline_index > 0)
          r_c = ctx.unfiltered_data[ctx.stride * (ctx.scanline_index - 1) + i - ctx.pixel_size];

        ctx.unfiltered_data[ctx.stride * ctx.scanline_index + i] += paeth_predictor(r_a, r_b, r_c);
      }
      break;
    }
    default:
      return;
      break;
    }
  }

  bool parse_idat(png_parse_context_s& ctx)
  {
    chunk_data_s cd;

    //sanity check    
    if(!check_next_chunk(ctx, cd)) return false; //does crc check too
    if(cd.type != chunk_type_idat) return false;
    if(cd.len < 7) return false;
    ctx.offset += 4 + 4; //len, type

    //allocate buffer for inflated data, can be calculated based on the ihdr data
    ctx.inflated_data = (uint8_t*)calloc(ctx.hdr.height * (1 + ctx.hdr.width * ctx.pixel_size), 1);
    if(ctx.inflated_data == NULL) return false;
    ctx.inflated_size = ctx.hdr.height * (1 + ctx.hdr.width * ctx.pixel_size);

    //uncompress data with the excellent tinf library
    int ret = tinf_uncompress(ctx.inflated_data, &ctx.inflated_size, ctx.data + ctx.offset + 2, cd.len);
    if(ret != TINF_OK)
    {
      free(ctx.inflated_data);
      ctx.inflated_data = NULL;
      ctx.inflated_size = 0;
      return false;
    }

    //check the inflated size if it matches the ihdr based size
    if(ctx.inflated_size != ctx.hdr.height * (1 + ctx.hdr.width * ctx.pixel_size))
    {
      free(ctx.inflated_data);
      ctx.inflated_data = NULL;
      ctx.inflated_size = 0;
      return false;
    }

    //calculate how much byte represents one scanline
    ctx.stride = ctx.hdr.width * ctx.pixel_size;

    //allocated buffor for the image representation
    ctx.unfiltered_data = (uint8_t*)calloc(ctx.hdr.height * ctx.stride, 1);
    if(ctx.unfiltered_data == NULL)
    {
      free(ctx.inflated_data);
      ctx.inflated_data = NULL;
      ctx.inflated_size = 0;
      return false;
    }
    ctx.unfiltered_size = ctx.hdr.height * ctx.stride;

    //unfilter the inflated data
    for(ctx.scanline_index = 0; ctx.scanline_index < ctx.hdr.height; ctx.scanline_index++)
      unfilter_scanline(ctx);

    //we don't need the infalted data buffer any more, deallocating it
    free(ctx.inflated_data);
    ctx.inflated_data = NULL;
    ctx.inflated_size = 0;

    //adjusting parsing offset
    ctx.offset += cd.len + 4; //data + crc32
    return true;
  }

  bool parse_iend(png_parse_context_s& ctx)
  {
    chunk_data_s cd;
    if(!check_next_chunk(ctx, cd)) return false; //does crc check too
    if(cd.type != chunk_type_iend) return false;
    if(cd.len != 0) return false; //IHDR must be 13 bytes long
    ctx.parsed = true;
    ctx.offset = ctx.size - 1;
    return true;
  }

  bool parse_next_chunk(png_parse_context_s& ctx)
  {
    if(ctx.parsed) return false;
    chunk_data_s cd;
    //peek the next chunk
    if(!check_next_chunk(ctx, cd)) return false; //does crc check too

    //call the specific chunk parser function
    switch (cd.type)
    {
    case chunk_type_ihdr: //header
    {
      if(!parse_ihdr(ctx)) return false;
      break;
    }
    case chunk_type_idat: //data
    {
      if(!parse_idat(ctx)) return false;
      break;
    }
    case chunk_type_iend: //end/terminator chunk
    {
      if(!parse_iend(ctx)) return false;
      break;
    }
    default:
      //unkown chunk, skip it
      ctx.offset += cd.len + 4 + 4 + 4; //skip entire chunk (len, type, data, crc32)
      break;
    }

    return true;
  }

  bool check_header(png_parse_context_s& ctx)
  {
    //sanity check of context
    if(ctx.size < 8) return false;
    if(ctx.offset != 0) return false;

    if(ctx.data[0] != 0x89) return false;

    //ASCII 'PNG'
    if(ctx.data[1] != 0x50) return false;
    if(ctx.data[2] != 0x4E) return false;
    if(ctx.data[3] != 0x47) return false;

    //DOS style line ending
    if(ctx.data[4] != 0x0D) return false;
    if(ctx.data[5] != 0x0A) return false;

    //DOS style EOF
    if(ctx.data[6] != 0x1A) return false;

    //UNIX style line ending
    if(ctx.data[7] != 0x0A) return false;

    //move the offset of the parsing
    ctx.offset += 8;
    return true;
  }

  bool init(png_parse_context_s& ctx, uint8_t* data, uint32_t len)
  {
    //zero everything in context
    memset(&ctx, 0, sizeof(ctx));

    //allooc memory for input data
    ctx.data = (uint8_t*)calloc(len, 1);
    if(ctx.data == NULL) return false;
    ctx.size = len;

    //copy input data to the context
    memcpy(ctx.data, data, ctx.size);
    return true;
  }

  void deinit(png_parse_context_s& ctx)
  {
    //free all allocated memory
    if(ctx.data) free(ctx.data);
    if(ctx.inflated_data) free(ctx.inflated_data);
    if(ctx.unfiltered_data) free(ctx.unfiltered_data);
    //zero everything
    memset(&ctx, 0, sizeof(ctx));
  }

  bool parse(png_parse_context_s& ctx)
  {
    //check the png header and the first ihdr
    if(!check_header(ctx)) return false;
    if(!parse_ihdr(ctx)) return false;

    //only supporting 32bit rgba or 24bit rgb pixel data
    if(ctx.hdr.bit_depth != 8) return false; 

    if(ctx.hdr.color_type == 6)
      ctx.pixel_size = 4;
    else if(ctx.hdr.color_type == 2)
      ctx.pixel_size = 3;
    else
      return false;

    //parse the following chunks until the first iend chunk is not found
    while (!ctx.parsed)
      if(!parse_next_chunk(ctx)) return false;

    //deallocate raw input buffer
    free(ctx.data);
    ctx.data = NULL;
    ctx.size = 0;
    return true;
  }
}
