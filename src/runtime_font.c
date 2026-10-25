/* libSceFont / libSceFontFt with the embedded DejaVu Sans (gpu/third_party/fonts, the overlay's
 * font) standing in for the PS4 system font sets, which are not shipped with games. Killzone
 * Shadow Fall measures and draws text through glyph outlines: GetCharGlyphMetrics gives pixel
 * metrics at the font's SetScalePixel size (y up, bearingY = top above the baseline), and
 * GenerateCharGlyph + GlyphRefersOutline give the outline in FreeType's layout (points in
 * pixels, tag 1 on-curve / 0 quadratic control, contour end indices), as shadPS4 does. */
#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <pthread.h>
#include <stdatomic.h>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wunused-parameter"
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "../gpu/third_party/imgui/imstb_truetype.h"
#pragma clang diagnostic pop

#define FONT_INVALID_PARAMETER ((int32_t)0x80460002)
#define FONT_INVALID_LIBRARY ((int32_t)0x80460004)
#define FONT_INVALID_FONT_HANDLE ((int32_t)0x80460005)
#define FONT_INVALID_GLYPH ((int32_t)0x80460006)
#define FONT_ALLOCATION_FAILED ((int32_t)0x80460010)
#define FONT_NO_SUPPORT_CODE ((int32_t)0x80460041)
#define FONT_NO_SUPPORT_GLYPH ((int32_t)0x80460042)
#define FONT_MAGIC 0x464f4e54u
#define GLYPH_MAGIC 0x0F03
#define MISSING_LOG_LIMIT 8

/* Fonts embedded by gpu/shim/gr2_overlay.cpp (weak: absent in builds without them): Noto Sans
 * for the system font sets (as shadPS4: with the wider DejaVu Sans, Killzone's text lost
 * letters), the overlay's DejaVu Sans otherwise. */
extern const unsigned char gr2_system_font_ttf[] __attribute__((weak));
extern const unsigned char gr2_font_ttf[] __attribute__((weak));

typedef struct { uint32_t magic; } Library;
typedef struct { uint32_t magic, set_type; float scale_w, scale_h; } Font;
typedef struct {
    float width, height;
    float h_bearing_x, h_bearing_y, h_advance;
    float v_bearing_x, v_bearing_y, v_advance;
} GlyphMetrics;
typedef struct {
    uint16_t id, reserved;
    uint16_t form_options;
    uint8_t glyph_form, metrics_form;
    const void *memory;
    void *reserved1, *reserved2;
} GenerateParams;
typedef struct { float x, y; } OutlinePoint;
typedef struct {
    int16_t contours, points;
    uint32_t flags;
    OutlinePoint *point;
    uint8_t *tag;
    uint16_t *contour_end;
} Outline;
/* OrbisFontGlyphOpaque first (the game reads it), then this runtime's data. */
typedef struct {
    uint16_t magic, flags;
    uint8_t glyph_form, metrics_form;
    uint16_t em_size, baseline, height_px, origin_x, origin_y;
    float scale_x, base_scale;
    const void *memory;
    GlyphMetrics metrics;
    Outline outline;
} Glyph;
_Static_assert(sizeof(GlyphMetrics)==32 && sizeof(GenerateParams)==32 && sizeof(Outline)==32 &&
               __builtin_offsetof(Glyph,memory)==24,"libSceFont layouts");

static Library library={FONT_MAGIC};
/* sceFontSelectLibraryFt hands back an opaque driver table the game passes on unchanged. */
static const uint64_t ft_driver_table[8];
static stbtt_fontinfo face;
static int face_state; /* 0 unknown, 1 ready, -1 unavailable */
static pthread_once_t face_once=PTHREAD_ONCE_INIT;
static _Atomic unsigned missing_logged;

static void load_face(void) {
    if (gr2_system_font_ttf && stbtt_InitFont(&face,gr2_system_font_ttf,stbtt_GetFontOffsetForIndex(gr2_system_font_ttf,0))) {
        face_state=1;
        puts("Runtime: system fonts: embedded Noto Sans stands in for the PS4 font sets");
    } else if (gr2_font_ttf && stbtt_InitFont(&face,gr2_font_ttf,stbtt_GetFontOffsetForIndex(gr2_font_ttf,0))) {
        face_state=1;
        puts("Runtime: system fonts: embedded DejaVu Sans stands in for the PS4 font sets");
    } else {
        face_state=-1;
        puts("Runtime: system fonts: no font available, glyphs unsupported");
    }
}
static int face_ready(void) { pthread_once(&face_once,load_face); return face_state>0; }
static uint16_t clamp_u16(float v) { return v<=0 ? 0 : v>=65535.0f ? 65535 : (uint16_t)lroundf(v); }

static ABI int32_t memory_init(void *memory,void *region,uint32_t size,const void *iface,void *mspace,
                               void *destroy,void *context) {
    (void)region; (void)size; (void)iface; (void)mspace; (void)destroy; (void)context;
    return memory ? 0 : FONT_INVALID_PARAMETER;
}
static ABI int32_t memory_term(void *memory) { return memory ? 0 : FONT_INVALID_PARAMETER; }
static ABI const void *select_library_ft(int32_t value) { return value ? NULL : ft_driver_table; }
static ABI int32_t create_library(const void *memory,const void *selection,uint64_t edition,Library **out) {
    (void)edition;
    if (!memory || !selection || !out) return FONT_INVALID_PARAMETER;
    *out=&library; return 0;
}
static ABI int32_t destroy_library(Library **lib) {
    if (!lib) return FONT_INVALID_PARAMETER;
    if (*lib!=&library) return FONT_INVALID_LIBRARY;
    *lib=NULL; return 0;
}
static ABI int32_t support_system_fonts(Library *lib) { return lib==&library ? 0 : FONT_INVALID_LIBRARY; }
static ABI int32_t open_font_set(Library *lib,uint32_t set_type,uint32_t mode,const void *params,Font **out) {
    (void)mode; (void)params;
    if (lib!=&library) return FONT_INVALID_LIBRARY;
    if (!out) return FONT_INVALID_PARAMETER;
    Font *f=calloc(1,sizeof(*f));
    if (!f) return FONT_ALLOCATION_FAILED;
    f->magic=FONT_MAGIC; f->set_type=set_type; f->scale_w=f->scale_h=16.0f;
    printf("Runtime: font set 0x%08X opened (%s)\n",set_type,face_ready() ? (gr2_system_font_ttf ? "Noto Sans" : "DejaVu Sans") : "no glyphs");
    *out=f; return 0;
}
static ABI int32_t close_font(Font *f) {
    if (!f || f->magic!=FONT_MAGIC) return FONT_INVALID_FONT_HANDLE;
    f->magic=0; free(f); return 0;
}
/* GR2_FONT_TRACE=1: every call on stderr (first 4000), to match missing letters to calls. */
static int font_trace(void) {
    static int v=-1;
    if (v<0) { const char *e=getenv("GR2_FONT_TRACE"); v=e && e[0]=='1'; }
    static _Atomic unsigned n;
    return v && atomic_fetch_add(&n,1)<4000;
}
static unsigned long trace_thread(void) {
#ifdef _WIN32
    return (unsigned long)pthread_self();
#else
    return (unsigned long)pthread_self();
#endif
}
static ABI int32_t set_scale_pixel(Font *f,float w,float h) {
    if (font_trace()) fprintf(stderr,"Font: thread %lx font %p scale %.2f x %.2f\n",trace_thread(),(void *)f,w,h);
    if (!f || f->magic!=FONT_MAGIC) return FONT_INVALID_FONT_HANDLE;
    f->scale_w=w; f->scale_h=h; return 0;
}
/* Glyph index of a code point, 0 when the font lacks it (logged a few times). */
static int glyph_index(uint32_t code) {
    int index=face_ready() ? stbtt_FindGlyphIndex(&face,(int)code) : 0;
    if (!index && atomic_fetch_add(&missing_logged,1)<MISSING_LOG_LIMIT)
        printf("Runtime: font has no glyph for U+%04X\n",code);
    return index;
}
static void measure(const Font *f,int index,GlyphMetrics *m) {
    float sx=stbtt_ScaleForMappingEmToPixels(&face,f->scale_w), sy=stbtt_ScaleForMappingEmToPixels(&face,f->scale_h);
    int advance,lsb,x0,y0,x1,y1;
    stbtt_GetGlyphHMetrics(&face,index,&advance,&lsb);
    if (!stbtt_GetGlyphBox(&face,index,&x0,&y0,&x1,&y1)) x0=y0=x1=y1=0; /* blank glyph (space) */
    memset(m,0,sizeof(*m));
    m->width=(float)(x1-x0)*sx; m->height=(float)(y1-y0)*sy;
    m->h_bearing_x=(float)x0*sx; m->h_bearing_y=(float)y1*sy; m->h_advance=(float)advance*sx;
}
static ABI int32_t char_glyph_metrics(Font *f,uint32_t code,GlyphMetrics *metrics) {
    if (!f || f->magic!=FONT_MAGIC) { if (metrics) memset(metrics,0,sizeof(*metrics)); return FONT_INVALID_FONT_HANDLE; }
    if (!metrics) return FONT_INVALID_PARAMETER;
    if (!code) { memset(metrics,0,sizeof(*metrics)); return FONT_NO_SUPPORT_CODE; }
    int index=glyph_index(code);
    if (!index) { memset(metrics,0,sizeof(*metrics)); return FONT_NO_SUPPORT_GLYPH; }
    measure(f,index,metrics);
    return 0;
}
/* The outline in pixels: stb's closing segment back to a contour's start is dropped (FreeType
 * contours close implicitly); cubic controls (CFF fonts) are tagged 2 like FreeType's. */
static int build_outline(Glyph *g,const Font *f,int index) {
    stbtt_vertex *v=NULL;
    int n=stbtt_GetGlyphShape(&face,index,&v);
    float sx=stbtt_ScaleForMappingEmToPixels(&face,f->scale_w), sy=stbtt_ScaleForMappingEmToPixels(&face,f->scale_h);
    OutlinePoint *pts=calloc((size_t)(2*n+1),sizeof(*pts));
    uint8_t *tags=calloc((size_t)(2*n+1),1);
    uint16_t *ends=calloc((size_t)(n+1),sizeof(*ends));
    if (!pts || !tags || !ends) { free(pts); free(tags); free(ends); stbtt_FreeShape(&face,v); return 0; }
    int np=0,nc=0,start=0;
    for (int i=0;i<n;++i) {
        if (v[i].type==STBTT_vmove) {
            if (np>start) ends[nc++]=(uint16_t)(np-1);
            start=np;
        } else if (v[i].type==STBTT_vcurve) {
            pts[np]=(OutlinePoint){v[i].cx*sx,v[i].cy*sy}; tags[np++]=0;
        } else if (v[i].type==STBTT_vcubic) {
            pts[np]=(OutlinePoint){v[i].cx*sx,v[i].cy*sy}; tags[np++]=2;
            pts[np]=(OutlinePoint){v[i].cx1*sx,v[i].cy1*sy}; tags[np++]=2;
        }
        pts[np]=(OutlinePoint){v[i].x*sx,v[i].y*sy}; tags[np++]=1;
        int last=i+1==n || v[i+1].type==STBTT_vmove;
        if (last && np-1>start && tags[np-1]==1 && pts[np-1].x==pts[start].x && pts[np-1].y==pts[start].y) --np;
    }
    if (np>start) ends[nc++]=(uint16_t)(np-1);
    stbtt_FreeShape(&face,v);
    g->outline=(Outline){(int16_t)nc,(int16_t)np,g->flags,pts,tags,ends};
    return 1;
}
static ABI int32_t generate_char_glyph(Font *f,uint32_t code,const GenerateParams *params,Glyph **out) {
    if (!out) return FONT_INVALID_PARAMETER;
    *out=NULL;
    if (!f || f->magic!=FONT_MAGIC) return FONT_INVALID_FONT_HANDLE;
    if (!code) return FONT_NO_SUPPORT_CODE;
    uint8_t glyph_form=params ? params->glyph_form : 0, metrics_form=params ? params->metrics_form : 0;
    uint16_t options=params ? params->form_options : 0;
    if ((options & ~0x11u) || glyph_form>1 || metrics_form>4 || (!glyph_form)!=(!metrics_form)) return FONT_INVALID_PARAMETER;
    int index=glyph_index(code);
    if (!index) return FONT_NO_SUPPORT_GLYPH;
    Glyph *g=calloc(1,sizeof(*g));
    if (!g) return FONT_ALLOCATION_FAILED;
    measure(f,index,&g->metrics);
    g->magic=GLYPH_MAGIC; g->flags=options; g->glyph_form=glyph_form; g->metrics_form=metrics_form;
    g->em_size=clamp_u16(f->scale_h); g->baseline=clamp_u16(g->metrics.h_bearing_y);
    g->height_px=clamp_u16(g->metrics.height); g->origin_y=g->baseline;
    g->scale_x=f->scale_w; g->base_scale=f->scale_h; g->memory=params ? params->memory : NULL;
    if (glyph_form==1 && !build_outline(g,f,index)) { free(g); return FONT_ALLOCATION_FAILED; }
    if (font_trace())
        fprintf(stderr,"Font: thread %lx font %p glyph '%c' U+%04X scale %.2f x %.2f: %d contours %d points, w %.1f h %.1f adv %.1f\n",
                trace_thread(),(void *)f,code<128 && code>=32 ? (char)code : '?',code,f->scale_w,f->scale_h,
                g->outline.contours,g->outline.points,g->metrics.width,g->metrics.height,g->metrics.h_advance);
    *out=g; return 0;
}
static ABI Outline *glyph_outline(Glyph *g) {
    return g && g->magic==GLYPH_MAGIC && g->glyph_form==1 ? &g->outline : NULL;
}
static ABI int32_t delete_glyph(const void *memory,Glyph **glyph) {
    (void)memory;
    if (!glyph) return FONT_INVALID_PARAMETER;
    Glyph *g=*glyph;
    if (!g || g->magic!=GLYPH_MAGIC) return FONT_INVALID_GLYPH;
    g->magic=0;
    free(g->outline.point); free(g->outline.tag); free(g->outline.contour_end); free(g);
    *glyph=NULL; return 0;
}

static const RuntimeExport exports[]={
    {"sceFontMemoryInit",memory_init}, {"sceFontMemoryTerm",memory_term},
    {"sceFontSelectLibraryFt",select_library_ft}, {"sceFontCreateLibraryWithEdition",create_library},
    {"sceFontDestroyLibrary",destroy_library}, {"sceFontSupportSystemFonts",support_system_fonts},
    {"sceFontOpenFontSet",open_font_set}, {"sceFontCloseFont",close_font},
    {"sceFontSetScalePixel",set_scale_pixel}, {"sceFontGetCharGlyphMetrics",char_glyph_metrics},
    {"sceFontGenerateCharGlyph",generate_char_glyph}, {"sceFontGlyphRefersOutline",glyph_outline},
    {"sceFontDeleteGlyph",delete_glyph},
};
uintptr_t runtime_font_resolve(const char *name) { return RUNTIME_LOOKUP(exports,name); }
