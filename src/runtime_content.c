/* Offline AppContent provider for the configured base-game profile.
 * Add-ons (DLC) come from GR2_ADDONS, set by run.py from <game>/addcont/<TITLE_ID>/<label>/ (the
 * folders shadPS4 reads): "LABEL=host folder|LABEL=host folder". They are reported installed, as
 * shadPS4 does; an add-on's folder is mounted at /addcontN when the game asks. No downloads; the
 * entitlement key is zero (unlock-key DLC carries no data to decrypt). */
#include "runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define INVALID_ID ((int32_t)0x805a1000)
#define NOT_LOADED ((int32_t)0x805a1001)
#define PARAMETER ((int32_t)0x80d90002)
#define BUSY ((int32_t)0x80d90003)
static unsigned refs, initialized, configured;
static uint32_t parameters[5];
static unsigned queries, lists, key_queries;

#define MAX_ADDONS 32
#define LABEL_SIZE 17
#define STATUS_INSTALLED 4
#define NOT_FOUND ((int32_t)0x80d90005)
#define NO_ENTITLEMENT ((int32_t)0x80d90007)
static struct { char label[LABEL_SIZE]; char host[512]; int mounted; } addons[MAX_ADDONS];
static unsigned addon_count;
typedef struct { char label[LABEL_SIZE]; char padding[3]; uint32_t status; } AddcontInfo;
_Static_assert(sizeof(AddcontInfo) == 24, "SceAppContentAddcontInfo is 24 bytes");

static void load_addons(void) {
    addon_count = 0;
    const char *spec = getenv("GR2_ADDONS");
    while (spec && *spec && addon_count < MAX_ADDONS) {
        const char *end = strchr(spec, '|');
        size_t length = end ? (size_t)(end - spec) : strlen(spec);
        const char *eq = memchr(spec, '=', length);
        size_t label = eq ? (size_t)(eq - spec) : length;
        if (label && label < LABEL_SIZE) {
            memcpy(addons[addon_count].label, spec, label);
            addons[addon_count].label[label] = 0;
            size_t host = eq ? length - label - 1 : 0;
            if (host >= sizeof(addons[0].host)) host = 0;
            memcpy(addons[addon_count].host, eq ? eq + 1 : "", host);
            addons[addon_count].host[host] = 0;
            addons[addon_count].mounted = -1;
            printf("Runtime: AppContent add-on %s (%s)\n", addons[addon_count].label,
                   host ? addons[addon_count].host : "no folder");
            ++addon_count;
        }
        spec = end ? end + 1 : NULL;
    }
}
static int find_addon(const void *label) {
    for (unsigned i = 0; i < addon_count; ++i)
        if (!strncmp((const char *)label, addons[i].label, LABEL_SIZE - 1)) return (int)i;
    return -1;
}
void runtime_content_configure(const uint32_t values[5]) {
    if (refs || initialized || (values[0]!=1 && values[0]!=3)) {
        fputs("ERROR: invalid or late content profile\n",stderr); exit(1);
    }
    memcpy(parameters,values,sizeof(parameters)); configured=1;
}
static void require_provider(void) {
    if (!configured) { fputs("STOP: AppContent needs --content-profile\n",stderr); exit(21); }
}
/* System modules: every library this eboot imports is provided by the host
 * runtime, so loading one only tracks a reference count. AppContent (0xb4)
 * additionally requires the configured content profile. */
static unsigned module_refs[0x10000];
static ABI int32_t module_load(uint16_t id) {
    if (!id) return INVALID_ID;
    if (id==0xb4) require_provider();
    if (module_refs[id]==UINT32_MAX) { fputs("STOP: module reference overflow\n",stderr); exit(21); }
    ++module_refs[id];
    if (id==0xb4) { refs=module_refs[id]; printf("Runtime: AppContent host provider loaded; references=%u\n",refs); }
    else printf("Runtime: system module 0x%x loaded (host implementation)\n",id);
    return 0;
}
static ABI int32_t module_loaded(uint16_t id) {
    if (!id) return INVALID_ID;
    return module_refs[id] ? 0 : NOT_LOADED;
}
static ABI int32_t module_unload(uint16_t id) {
    int32_t result=module_loaded(id);
    if (result) return result;
    --module_refs[id];
    if (id==0xb4) { refs=module_refs[id]; if (!refs) initialized=0; }
    return 0;
}
static void require_loaded(void) {
    require_provider();
    if (!refs) { fputs("STOP: AppContent called before module load\n",stderr); exit(21); }
}
static ABI int32_t content_init(const unsigned char *init,unsigned char *boot) {
    require_loaded();
    if (!init || !boot) return PARAMETER;
    if (initialized) return BUSY;
    /* Only the reserved-zero init profile is implemented. */
    for (unsigned i=0;i<32;++i) if (init[i]) return PARAMETER;
    memset(boot,0,40); initialized=1;
    load_addons();
    printf("Runtime: AppContent initialized; offline base-game profile, %u add-on(s)\n", addon_count);
    return 0;
}
static void require_initialized(void) {
    require_loaded();
    if (!initialized) { fputs("STOP: AppContent query before initialization\n",stderr); exit(21); }
}
static ABI int32_t param_int(uint32_t id,int32_t *out) {
    require_initialized();
    if (id>4 || !out) return PARAMETER;
    memcpy(out,&parameters[id],4); ++queries;
    printf("Runtime: AppContent parameter %u = %d\n",id,*out);
    return 0;
}
static ABI int32_t addon_list(uint32_t service,void *list,uint32_t capacity,uint32_t *hits) {
    require_initialized();
    if ((!capacity || !list) && !hits) return PARAMETER;
    /* The add-ons belong to the game's own service (label 0), as in shadPS4; other service labels
     * have none. Without a list only the count is wanted. Never touch unused list slots. */
    uint32_t count = service == 0 ? addon_count : 0, written = 0;
    if (list && capacity) {
        AddcontInfo *out = list;
        for (; written < count && written < capacity; ++written) {
            memset(&out[written], 0, sizeof(out[written]));
            memcpy(out[written].label, addons[written].label, LABEL_SIZE);
            out[written].status = STATUS_INSTALLED;
        }
    } else {
        written = count;
    }
    if (hits) *hits = written;
    if (lists++ < 4) printf("Runtime: AppContent add-on list (service %u): %u\n", service, written);
    return 0;
}
/* Temporary data lives in /temp0 (runtime_file.c maps it to the user directory). */
static ABI int32_t temp_mount(uint32_t option,char *mount_point) {
    (void)option;
    require_initialized();
    if (!mount_point) return PARAMETER;
    memcpy(mount_point,"/temp0",7);
    puts("Runtime: AppContent temporary data mounted at /temp0");
    return 0;
}
static ABI int32_t temp_space(const char *mount_point,uint64_t *kb) {
    require_initialized();
    if (!mount_point || !kb) return PARAMETER;
    *kb=1024*1024; return 0; /* 1 GiB free */
}
/* An add-on's folder is mounted at /addcontN (N: its place in the list). */
static ABI int32_t addon_mount(uint32_t service,const void *label,char *mount_point) {
    require_initialized();
    if (!label || !mount_point || service > 7) return PARAMETER;
    int i = service == 0 ? find_addon(label) : -1;
    if (i < 0 || !addons[i].host[0]) return NOT_FOUND;
    char guest[24];
    snprintf(guest, sizeof(guest), "/addcont%u", (unsigned)i % MAX_ADDONS);
    if (addons[i].mounted < 0) {
        if (runtime_file_mount(guest, addons[i].host)) return NOT_FOUND;
        addons[i].mounted = i;
        printf("Runtime: AppContent add-on %s mounted at %s\n", addons[i].label, guest);
    }
    memcpy(mount_point, guest, strlen(guest) + 1);
    return 0;
}
static ABI int32_t entitlement_key(uint32_t service,const void *label,void *key) {
    require_initialized();
    if (!label || !key) return PARAMETER;
    int found = service == 0 ? find_addon(label) : -1;
    if (key_queries++ < 8)
        printf("Runtime: AppContent entitlement key %.16s: %s\n", (const char *)label, found < 0 ? "not entitled" : "entitled");
    if (found < 0) return NO_ENTITLEMENT;
    memset(key, 0, 16);
    return 0;
}
uintptr_t runtime_content_resolve(const char *name) {
    if (!strcmp(name,"buYbeLOGWmA#libSceAppContent")) return (uintptr_t)temp_mount;
    if (!strcmp(name,"SaKib2Ug0yI#libSceAppContent")) return (uintptr_t)temp_space;
    if (!strcmp(name,"VANhIWcqYak#libSceAppContent")) return (uintptr_t)addon_mount;
    if (!strcmp(name,"XTWR0UXvcgs#libSceAppContent")) return (uintptr_t)entitlement_key;
    if (!strcmp(name,"g8cM39EUZ6o#libSceSysmodule")) return (uintptr_t)module_load;
    if (!strcmp(name,"fMP5NHUOaMk#libSceSysmodule")) return (uintptr_t)module_loaded;
    if (!strcmp(name,"eR2bZFAAU0Q#libSceSysmodule")) return (uintptr_t)module_unload;
    if (!strcmp(name,"R9lA82OraNs#libSceAppContent")) return (uintptr_t)content_init;
    if (!strcmp(name,"99b82IKXpH4#libSceAppContent")) return (uintptr_t)param_int;
    if (!strcmp(name,"xnd8BJzAxmk#libSceAppContent")) return (uintptr_t)addon_list;
    return 0;
}
void runtime_content_report(void) {
    printf("Runtime: AppContent references=%u, initialized=%u, parameter queries=%u, list queries=%u, add-ons=%u, key queries=%u\n",
           refs,initialized,queries,lists,addon_count,key_queries);
}
