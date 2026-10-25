/* Modules the game loads itself. link_modules.py --dynamic places and relocates them in the
 * image ahead of time, so sceKernelLoadStartModule only hands out a handle (these modules
 * have no initializers; the linker refuses ones that do) and sceKernelDlsym looks a symbol's
 * NID up in the module's export table. A symbol name is turned into its NID like the PS4
 * does: SHA-1 of name + salt, first 8 bytes as a little-endian value, 11 base64 digits. */
#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define ERR(n) ((int32_t)(UINT32_C(0x80020000)|(n)))
#define MAX_MODULES 8
#define HANDLE_BASE 0x100

typedef struct { RuntimeDynamicModule info; unsigned char *image; unsigned loads; } Module;
static Module modules[MAX_MODULES];
static unsigned module_count;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;

void runtime_module_register(const RuntimeDynamicModule *module, unsigned char *image) {
    if (module_count==MAX_MODULES) { fputs("STOP: too many dynamic modules\n",stderr); exit(21); }
    modules[module_count].info=*module; modules[module_count].image=image;
    printf("Runtime: dynamic module %s at image offset 0x%llx (%llu exports)\n",module->path,
           (unsigned long long)module->base,(unsigned long long)module->export_count);
    ++module_count;
}

static uint32_t rol(uint32_t v,int n) { return v<<n | v>>(32-n); }
static void sha1(const unsigned char *data,size_t size,unsigned char out[20]) {
    uint32_t h[5]={0x67452301,0xEFCDAB89,0x98BADCFE,0x10325476,0xC3D2E1F0};
    size_t padded=(size+9+63)/64*64;
    unsigned char *m=calloc(1,padded);
    if (!m) { fputs("Cannot allocate SHA-1 buffer\n",stderr); exit(1); }
    memcpy(m,data,size); m[size]=0x80;
    for (int i=0;i<8;++i) m[padded-1-i]=(unsigned char)((uint64_t)size*8>>(8*i));
    for (size_t block=0;block<padded;block+=64) {
        uint32_t w[80];
        for (int i=0;i<16;++i) w[i]=(uint32_t)m[block+4*i]<<24|m[block+4*i+1]<<16|m[block+4*i+2]<<8|m[block+4*i+3];
        for (int i=16;i<80;++i) w[i]=rol(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4];
        for (int i=0;i<80;++i) {
            uint32_t f,k;
            if (i<20) { f=(b&c)|(~b&d); k=0x5A827999; }
            else if (i<40) { f=b^c^d; k=0x6ED9EBA1; }
            else if (i<60) { f=(b&c)|(b&d)|(c&d); k=0x8F1BBCDC; }
            else { f=b^c^d; k=0xCA62C1D6; }
            uint32_t t=rol(a,5)+f+e+k+w[i];
            e=d; d=c; c=rol(b,30); b=a; a=t;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e;
    }
    free(m);
    for (int i=0;i<20;++i) out[i]=(unsigned char)(h[i/4]>>(24-8*(i%4)));
}
static void symbol_nid(const char *name,char nid[12]) {
    static const unsigned char salt[16]={0x51,0x8D,0x64,0xA6,0x35,0xDE,0xD8,0xC1,0xE6,0xB0,0x39,0xB1,0xC3,0xE5,0x52,0x30};
    static const char codes[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-";
    size_t n=strlen(name);
    unsigned char *input=malloc(n+16), hash[20];
    if (!input) { fputs("Cannot allocate NID buffer\n",stderr); exit(1); }
    memcpy(input,name,n); memcpy(input+n,salt,16);
    sha1(input,n+16,hash);
    free(input);
    uint64_t digest=0;
    for (int i=7;i>=0;--i) digest=digest<<8|hash[i];
    for (int i=0;i<10;++i) nid[i]=codes[(digest>>(58-i*6))&0x3f];
    nid[10]=codes[(digest&0xf)*4]; nid[11]=0;
}
static const RuntimeDynamicExport *find_export(const Module *m,const char *nid) {
    size_t lo=0,hi=m->info.export_count;
    while (lo<hi) {
        size_t mid=(lo+hi)/2;
        int c=strcmp(m->info.exports[mid].nid,nid);
        if (!c) return &m->info.exports[mid];
        if (c<0) lo=mid+1; else hi=mid;
    }
    return NULL;
}
/* "/app0/x.prx", "x.prx" (relative to /app0) and other spellings of the same file match. */
static Module *find_module(const char *path) {
    const char *base=strrchr(path,'/');
    base=base ? base+1 : path;
    for (unsigned i=0;i<module_count;++i) {
        const char *own=strrchr(modules[i].info.path,'/')+1;
        if (!strcmp(path,modules[i].info.path) || (!strchr(path,'/') && !strcmp(base,own)) ||
            (!strncmp(path,"/app0/",6) && !strcmp(base,own) && strchr(path+6,'/')==NULL)) return &modules[i];
    }
    return NULL;
}
static Module *from_handle(int32_t handle) {
    unsigned index=(unsigned)(handle-HANDLE_BASE);
    return handle>=HANDLE_BASE && index<module_count ? &modules[index] : NULL;
}

/* GR2_STATIC_MODULES ("/app0/sysdata/libSceSecure.prx;..."): modules the game loads itself that
 * are linked into the image and already started at boot (run.py). Loading one succeeds with a
 * handle of its own; its exports are bound to the game's imports, so no sceKernelDlsym. */
#define STATIC_HANDLE_BASE 0x1000
static int32_t static_module(const char *path) {
    const char *list=getenv("GR2_STATIC_MODULES");
    if (!list || !path) return 0;
    size_t n=strlen(path);
    int32_t index=0;
    for (const char *p=list;*p;++index) {
        const char *end=strchr(p,';');
        size_t len=end ? (size_t)(end-p) : strlen(p);
        if (len==n && !strncmp(p,path,n)) return STATIC_HANDLE_BASE+index;
        if (!end) break;
        p=end+1;
    }
    return 0;
}
static ABI int32_t load_start_module(const char *path,uint64_t args,const void *argp,uint32_t flags,
                                     const void *options,int32_t *result) {
    (void)args; (void)argp; (void)options;
    if (!path) return ERR(14);
    if (flags) { fprintf(stderr,"STOP: sceKernelLoadStartModule flags 0x%x\n",flags); exit(21); }
    pthread_mutex_lock(&lock);
    Module *m=find_module(path);
    if (m) ++m->loads;
    pthread_mutex_unlock(&lock);
    if (!m) {
        int32_t linked=static_module(path);
        if (linked) {
            printf("Runtime: sceKernelLoadStartModule(%s) -> handle 0x%x (linked and started at boot)\n",path,linked);
            if (result) *result=0;
            return linked;
        }
        printf("Runtime: sceKernelLoadStartModule(%s): not linked into the image (link_modules.py --dynamic)\n",path);
        return ERR(2); /* ENOENT */
    }
    printf("Runtime: sceKernelLoadStartModule(%s) -> handle 0x%x\n",path,HANDLE_BASE+(unsigned)(m-modules));
    if (result) *result=0; /* module_start: none */
    return HANDLE_BASE+(int32_t)(m-modules);
}
static ABI int32_t stop_unload_module(int32_t handle,uint64_t args,const void *argp,uint32_t flags,
                                      const void *options,int32_t *result) {
    (void)args; (void)argp; (void)flags; (void)options;
    if (handle>=STATIC_HANDLE_BASE && handle<STATIC_HANDLE_BASE+64) { if (result) *result=0; return 0; }
    pthread_mutex_lock(&lock);
    Module *m=from_handle(handle);
    int known=m && m->loads;
    if (known) --m->loads; /* the image keeps it mapped: a later load returns the same code */
    pthread_mutex_unlock(&lock);
    if (!known) { printf("Runtime: sceKernelStopUnloadModule(%d): no such module\n",handle); return ERR(3); }
    if (result) *result=0;
    return 0;
}
static ABI int32_t module_dlsym(int32_t handle,const char *symbol,void **address) {
    Module *m=from_handle(handle);
    if (!m) return ERR(3);
    if (!symbol || !address) return ERR(22);
    char nid[12];
    symbol_nid(symbol,nid);
    const RuntimeDynamicExport *e=find_export(m,nid);
    if (!e && strlen(symbol)==11) e=find_export(m,symbol); /* already a NID */
    if (!e) {
        printf("Runtime: sceKernelDlsym(%s, %s): not exported\n",m->info.path,symbol);
        return ERR(3);
    }
    *address=m->image+e->address;
    return 0;
}

static const RuntimeExport exports[]={
    {"sceKernelLoadStartModule",load_start_module}, {"sceKernelStopUnloadModule",stop_unload_module},
    {"sceKernelDlsym",module_dlsym},
};
uintptr_t runtime_module_resolve(const char *name) { return RUNTIME_LOOKUP(exports,name); }
