/* libScePngDec on libpng's simplified API: images decode to 8-bit RGBA or BGRA rows at the
 * caller's pitch; images without alpha get the caller's alpha value. The decoder handle is
 * only a validated tag in the caller's work memory (libpng keeps no state between calls). */
#include "runtime.h"
#include <stdio.h>
#include <string.h>
#include <png.h>

#define PNG_INVALID_ADDR ((int32_t)0x80690001)
#define PNG_INVALID_SIZE ((int32_t)0x80690002)
#define PNG_INVALID_PARAM ((int32_t)0x80690003)
#define PNG_INVALID_HANDLE ((int32_t)0x80690004)
#define PNG_INVALID_DATA ((int32_t)0x80690010)
#define PNG_DECODE_ERROR ((int32_t)0x80690012)
#define HANDLE_MAGIC 0x504e4744u

typedef struct { uint32_t this_size, attribute, max_width; } CreateParam;
typedef struct { const unsigned char *png; uint32_t png_size, reserved; } ParseParam;
typedef struct {
    const unsigned char *png; unsigned char *image;
    uint32_t png_size, image_size;
    uint16_t pixel_format, alpha; /* 0 = R8G8B8A8, 1 = B8G8R8A8 */
    uint32_t pitch;
} DecodeParam;
typedef struct { uint32_t width, height; uint16_t color_space, bit_depth; uint32_t flags; } ImageInfo;
_Static_assert(sizeof(DecodeParam)==32 && sizeof(ImageInfo)==16,"PngDec layouts");
typedef struct { uint32_t magic, max_width; } Decoder;

/* Header fields straight from IHDR (and whether a tRNS chunk precedes IDAT). */
static int32_t read_info(const unsigned char *png,uint32_t size,ImageInfo *info) {
    static const unsigned char signature[8]={137,'P','N','G',13,10,26,10};
    if (size<33 || memcmp(png,signature,8) || memcmp(png+12,"IHDR",4)) return PNG_INVALID_DATA;
    const unsigned char *h=png+16;
    uint8_t color=h[9];
    static const uint16_t spaces[7]={2,0,3,4,18,0,19}; /* PNG color type -> Orbis color space */
    if (color>6 || (color!=0 && !spaces[color])) return PNG_INVALID_DATA;
    info->width=(uint32_t)h[0]<<24|h[1]<<16|h[2]<<8|h[3];
    info->height=(uint32_t)h[4]<<24|h[5]<<16|h[6]<<8|h[7];
    info->bit_depth=h[8]; info->color_space=spaces[color];
    info->flags=h[12] ? 1 : 0; /* Adam7 interlace */
    for (uint32_t at=8; at+8<=size;) {
        uint32_t length=(uint32_t)png[at]<<24|png[at+1]<<16|png[at+2]<<8|png[at+3];
        if (!memcmp(png+at+4,"tRNS",4)) { info->flags|=2; break; }
        if (!memcmp(png+at+4,"IDAT",4) || length>size-at-8) break;
        at+=12+length;
    }
    return 0;
}
static ABI int32_t png_query_memory(const CreateParam *param) {
    if (!param) return PNG_INVALID_PARAM;
    if (param->attribute>1) return PNG_INVALID_ADDR;
    if (param->max_width-1>1000000) return PNG_INVALID_SIZE;
    return (int32_t)sizeof(Decoder);
}
static ABI int32_t png_create(const CreateParam *param,void *memory,uint32_t size,Decoder **handle) {
    if (!param || param->attribute>1) return PNG_INVALID_PARAM;
    if (!memory || !handle) return PNG_INVALID_ADDR;
    if (param->max_width-1>1000000 || size<sizeof(Decoder)) return PNG_INVALID_SIZE;
    Decoder *d=memory;
    d->magic=HANDLE_MAGIC; d->max_width=param->max_width;
    *handle=d; return 0;
}
static ABI int32_t png_delete(Decoder *d) {
    if (!d || d->magic!=HANDLE_MAGIC) return PNG_INVALID_HANDLE;
    d->magic=0; return 0;
}
static ABI int32_t png_parse_header(const ParseParam *param,ImageInfo *info) {
    if (!param || !info) return PNG_INVALID_PARAM;
    if (!param->png) return PNG_INVALID_ADDR;
    return read_info(param->png,param->png_size,info);
}
/* Returns width<<16 | height (0 for images too large for that encoding). */
static ABI int32_t png_decode(Decoder *d,const DecodeParam *param,ImageInfo *out_info) {
    if (!d || d->magic!=HANDLE_MAGIC) return PNG_INVALID_HANDLE;
    if (!param) return PNG_INVALID_PARAM;
    if (!param->png || !param->image) return PNG_INVALID_ADDR;
    ImageInfo info;
    int32_t e=read_info(param->png,param->png_size,&info);
    if (e) return e;
    if (info.width>d->max_width) return PNG_INVALID_SIZE;
    if (out_info) *out_info=info;
    png_image image;
    memset(&image,0,sizeof(image));
    image.version=PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&image,param->png,param->png_size)) {
        fprintf(stderr,"Runtime: PngDec: %s\n",image.message);
        return PNG_DECODE_ERROR;
    }
    int has_alpha=(image.format & PNG_FORMAT_FLAG_ALPHA)!=0;
    image.format=param->pixel_format==1 ? PNG_FORMAT_BGRA : PNG_FORMAT_RGBA;
    uint32_t pitch=param->pitch ? param->pitch : image.width*4;
    if (pitch<image.width*4 || (uint64_t)pitch*image.height>param->image_size) {
        png_image_free(&image);
        return PNG_INVALID_SIZE;
    }
    if (!png_image_finish_read(&image,NULL,param->image,(png_int_32)pitch,NULL)) {
        fprintf(stderr,"Runtime: PngDec: %s\n",image.message);
        return PNG_DECODE_ERROR;
    }
    if (!has_alpha) {
        uint8_t alpha=(uint8_t)(param->alpha>255 ? 255 : param->alpha);
        for (uint32_t y=0;y<image.height;++y)
            for (uint32_t x=0;x<image.width;++x) param->image[(size_t)y*pitch+x*4+3]=alpha;
    }
    return image.width>32767 || image.height>32767 ? 0 : (int32_t)(image.width<<16 | image.height);
}

static const RuntimeExport exports[]={
    {"scePngDecQueryMemorySize",png_query_memory}, {"scePngDecCreate",png_create},
    {"scePngDecDelete",png_delete}, {"scePngDecParseHeader",png_parse_header},
    {"scePngDecDecode",png_decode},
};
uintptr_t runtime_png_resolve(const char *name) { return RUNTIME_LOOKUP(exports,name); }
