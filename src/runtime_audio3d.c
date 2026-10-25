/* libSceAudio3d: Gravity Rush 2 sends all of its sound through 3D audio objects (mono PCM with a
 * gain and a position each, rendered by libSceNgs2). Follows shadPS4's audio3d.cpp
 * (GPL-2.0-or-later): every sceAudio3dPortAdvance mixes one granule of the bed and of every
 * object into stereo, and sceAudio3dPortPush plays the mixed granules through an AudioOut port
 * (type 126), which paces the game's audio thread. Objects are panned left/right from their
 * position (shadPS4 mixes them to the center); GR2_AUDIO3D_PAN=0 turns panning off, -1
 * swaps the sides. */
#include "runtime.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define A3D_UNKNOWN ((int32_t)0x80EA0001)
#define A3D_INVALID_PORT ((int32_t)0x80EA0002)
#define A3D_INVALID_OBJECT ((int32_t)0x80EA0003)
#define A3D_INVALID_PARAMETER ((int32_t)0x80EA0004)
#define A3D_OUT_OF_MEMORY ((int32_t)0x80EA0005)
#define A3D_OUT_OF_RESOURCES ((int32_t)0x80EA0006)
#define A3D_NOT_READY ((int32_t)0x80EA0007)
#define A3D_NOT_SUPPORTED ((int32_t)0x80EA0008)

enum { ATTR_PCM = 1, ATTR_POSITION = 2, ATTR_GAIN = 3, ATTR_SPREAD = 4, ATTR_PASSTHROUGH = 6, ATTR_RESET_STATE = 9 };
enum { MODE_NO_ADVANCE = 0, MODE_ADVANCE_NO_PUSH = 1, MODE_ADVANCE_AND_PUSH = 2 };
enum { FORMAT_S16 = 0, FORMAT_FLOAT = 1 };
#define MAX_PORTS 4
#define MAX_QUEUE 64
#define AUDIO_OUT_TYPE_AUDIO3D 126
#define AUDIO_OUT_S16_STEREO 1

typedef struct { uint64_t size_this; uint32_t granularity, rate, max_objects, queue_depth, buffer_mode, pad, num_beds; } OpenParameters;
typedef struct { uint32_t format; uint32_t pad; const void *buffer; uint32_t num_samples; } Pcm;
typedef struct { uint32_t id; uint32_t pad; const void *value; uint64_t size; } Attribute;

/* A granule of samples (always `granularity` frames; shorter input is zero-padded). */
typedef struct { void *data; uint32_t channels, format; } Granule;
typedef struct { Granule items[MAX_QUEUE]; unsigned head, count; } Queue;
typedef struct {
    uint32_t id;
    int used, unreserved, has_position, passthrough;
    float gain, position[3];
    Queue pcm;
} Object;
typedef struct {
    int used;
    pthread_mutex_t lock;
    OpenParameters params;
    int32_t audio_out;
    Object *objects;
    uint32_t object_capacity, next_object_id;
    Queue bed, mixed;
} Port;

static pthread_mutex_t ports_lock = PTHREAD_MUTEX_INITIALIZER;
static Port ports[MAX_PORTS];
static int initialized, pan_mode = 1;
static unsigned long long granules_mixed, granules_pushed;

static void queue_clear(Queue *q) {
    while (q->count) { free(q->items[q->head].data); q->head = (q->head + 1) % MAX_QUEUE; --q->count; }
    q->head = 0;
}
static int queue_push(Queue *q, Granule g) {
    if (q->count == MAX_QUEUE) return 0;
    q->items[(q->head + q->count) % MAX_QUEUE] = g;
    ++q->count;
    return 1;
}
static Granule queue_pop(Queue *q) {
    Granule g = q->items[q->head];
    q->head = (q->head + 1) % MAX_QUEUE;
    --q->count;
    return g;
}

static Port *port_get(uint32_t id) { return id < MAX_PORTS && ports[id].used ? &ports[id] : NULL; }
static Object *object_get(Port *p, uint32_t id) {
    for (uint32_t i = 0; i < p->object_capacity; ++i)
        if (p->objects[i].used && p->objects[i].id == id) return &p->objects[i];
    return NULL;
}

static int32_t enqueue(Queue *q, const Pcm *pcm, uint32_t channels, uint32_t granularity, uint32_t depth) {
    if (!pcm->buffer || !pcm->num_samples || pcm->format > FORMAT_FLOAT) return A3D_INVALID_PARAMETER;
    uint32_t bytes = pcm->format == FORMAT_S16 ? 2 : 4;
    if ((uintptr_t)pcm->buffer & (bytes - 1)) return A3D_INVALID_PARAMETER;
    if (q->count >= depth) return A3D_NOT_READY;
    void *copy = calloc(1, (size_t)granularity * channels * bytes);
    if (!copy) return A3D_OUT_OF_MEMORY;
    uint32_t n = pcm->num_samples < granularity ? pcm->num_samples : granularity;
    memcpy(copy, pcm->buffer, (size_t)n * channels * bytes);
    if (!queue_push(q, (Granule){copy, channels, pcm->format})) { free(copy); return A3D_NOT_READY; }
    return 0;
}

/* Left/right gains of an object: constant power from the azimuth of its position. */
static void object_pan(const Object *o, float *left, float *right) {
    *left = *right = 0.70710678f;
    if (!pan_mode || !o->has_position || o->passthrough) return;
    float x = o->position[0] * (float)pan_mode, z = o->position[2];
    float d = sqrtf(x * x + z * z);
    if (d < 1e-4f) return;
    float pan = x / d; /* -1 left .. 1 right */
    float angle = (pan + 1.0f) * 0.78539816f;
    *left = cosf(angle);
    *right = sinf(angle);
}

static void mix_granule(float *mix, uint32_t frames, Granule g, float gain_l, float gain_r) {
    for (uint32_t i = 0; i < frames; ++i) {
        float s[8] = {0};
        uint32_t c = g.channels > 8 ? 8 : g.channels;
        for (uint32_t k = 0; k < c; ++k)
            s[k] = g.format == FORMAT_S16 ? ((const int16_t *)g.data)[i * g.channels + k] / 32768.0f
                                          : ((const float *)g.data)[i * g.channels + k];
        float l, r;
        if (c == 1) l = r = s[0];
        else {
            /* L R C LFE SL SR BL BR folded to stereo: fronts full, center and surrounds -3 dB. */
            l = s[0]; r = s[1];
            if (c >= 3) { l += 0.7071f * s[2]; r += 0.7071f * s[2]; }
            if (c >= 6) { l += 0.7071f * s[4]; r += 0.7071f * s[5]; }
            if (c >= 8) { l += 0.7071f * s[6]; r += 0.7071f * s[7]; }
        }
        mix[i * 2] += l * gain_l;
        mix[i * 2 + 1] += r * gain_r;
    }
}

static int32_t mix(Port *p) {
    uint32_t frames = p->params.granularity;
    float *acc = calloc((size_t)frames * 2, sizeof(float));
    int16_t *out = malloc((size_t)frames * 2 * sizeof(int16_t));
    if (!acc || !out) { free(acc); free(out); return A3D_OUT_OF_MEMORY; }
    if (p->bed.count) {
        Granule g = queue_pop(&p->bed);
        mix_granule(acc, frames, g, 1.0f, 1.0f);
        free(g.data);
    }
    for (uint32_t i = 0; i < p->object_capacity; ++i) {
        Object *o = &p->objects[i];
        if (!o->used) continue;
        if (o->pcm.count) {
            Granule g = queue_pop(&o->pcm);
            if (o->gain != 0.0f) {
                float l, r;
                object_pan(o, &l, &r);
                /* Mono objects keep their level at the center (constant power pan * sqrt 2). */
                mix_granule(acc, frames, g, o->gain * l * 1.41421356f, o->gain * r * 1.41421356f);
            }
            free(g.data);
        }
        if (o->unreserved && !o->pcm.count) o->used = 0;
    }
    for (uint32_t i = 0; i < frames * 2; ++i) {
        float v = acc[i] < -1.0f ? -1.0f : acc[i] > 1.0f ? 1.0f : acc[i];
        out[i] = (int16_t)(v * 32767.0f);
    }
    free(acc);
    if (!queue_push(&p->mixed, (Granule){out, 2, FORMAT_S16})) { free(out); return A3D_NOT_READY; }
    ++granules_mixed;
    return 0;
}

static ABI int32_t a3d_initialize(int64_t reserved) {
    if (reserved) return A3D_INVALID_PARAMETER;
    pthread_mutex_lock(&ports_lock);
    int was = initialized;
    initialized = 1;
    const char *pan = getenv("GR2_AUDIO3D_PAN");
    if (pan) pan_mode = atoi(pan) < 0 ? -1 : atoi(pan) > 0 ? 1 : 0;
    pthread_mutex_unlock(&ports_lock);
    if (was) return A3D_NOT_READY;
    printf("Runtime: Audio3d initialized (objects mixed to stereo, panning %s)\n",
           pan_mode ? (pan_mode > 0 ? "on" : "swapped") : "off");
    return 0;
}

static ABI int32_t a3d_terminate(void) {
    pthread_mutex_lock(&ports_lock);
    initialized = 0;
    pthread_mutex_unlock(&ports_lock);
    return 0;
}

static ABI int32_t a3d_default_parameters(OpenParameters *params) {
    if (params) {
        OpenParameters d = {.size_this = 0x20, .granularity = 0x100, .rate = 0, .max_objects = 512,
                            .queue_depth = 2, .buffer_mode = MODE_ADVANCE_AND_PUSH};
        memcpy(params, &d, 0x20);
    }
    return 0;
}

static ABI int32_t a3d_port_open(int32_t user, const OpenParameters *in, uint32_t *port_id) {
    (void)user;
    if (!in || !port_id) { if (port_id) *port_id = UINT32_MAX; return A3D_INVALID_PARAMETER; }
    *port_id = UINT32_MAX;
    if (!initialized) return A3D_NOT_READY;
    OpenParameters e = {.size_this = 0x28, .granularity = in->granularity, .rate = in->rate, .max_objects = 512,
                        .queue_depth = 2, .buffer_mode = MODE_NO_ADVANCE, .num_beds = 2};
    switch (in->size_this & ~UINT64_C(7)) {
    case 0x10: break;
    case 0x18: e.max_objects = in->max_objects; e.queue_depth = in->queue_depth; e.buffer_mode = MODE_ADVANCE_NO_PUSH; break;
    case 0x20: e.max_objects = in->max_objects; e.queue_depth = in->queue_depth; e.buffer_mode = in->buffer_mode; break;
    case 0x28:
        e.max_objects = in->max_objects; e.queue_depth = in->queue_depth; e.buffer_mode = in->buffer_mode;
        e.num_beds = in->num_beds; break;
    default: return A3D_INVALID_PARAMETER;
    }
    if (e.rate != 0 || e.granularity < 0x100 || (e.granularity & 0xff) || e.granularity > 2048 || !e.max_objects ||
        !e.queue_depth || e.queue_depth > MAX_QUEUE || e.buffer_mode > 2)
        return A3D_INVALID_PARAMETER;
    if (e.max_objects > 512) e.max_objects = 512;
    pthread_mutex_lock(&ports_lock);
    uint32_t id = UINT32_MAX;
    for (uint32_t i = 0; i < MAX_PORTS; ++i) if (!ports[i].used) { id = i; break; }
    if (id == UINT32_MAX) { pthread_mutex_unlock(&ports_lock); return A3D_OUT_OF_RESOURCES; }
    Port *p = &ports[id];
    memset(p, 0, sizeof(*p));
    p->objects = calloc(e.max_objects, sizeof(Object));
    if (!p->objects) { pthread_mutex_unlock(&ports_lock); return A3D_OUT_OF_MEMORY; }
    pthread_mutex_init(&p->lock, NULL);
    p->params = e;
    p->object_capacity = e.max_objects;
    p->audio_out = -1;
    p->used = 1;
    pthread_mutex_unlock(&ports_lock);
    *port_id = id;
    printf("Runtime: Audio3d port %u opened (granularity %u, %u objects, queue %u, mode %u)\n", id, e.granularity,
           e.max_objects, e.queue_depth, e.buffer_mode);
    return 0;
}

static ABI int32_t a3d_port_close(uint32_t port_id) {
    pthread_mutex_lock(&ports_lock);
    Port *p = port_get(port_id);
    if (!p) { pthread_mutex_unlock(&ports_lock); return A3D_INVALID_PORT; }
    pthread_mutex_lock(&p->lock);
    if (p->audio_out >= 0) runtime_audio_close_port(p->audio_out);
    queue_clear(&p->bed);
    queue_clear(&p->mixed);
    for (uint32_t i = 0; i < p->object_capacity; ++i) queue_clear(&p->objects[i].pcm);
    free(p->objects);
    p->used = 0;
    pthread_mutex_unlock(&p->lock);
    pthread_mutex_destroy(&p->lock);
    pthread_mutex_unlock(&ports_lock);
    return 0;
}

static ABI int32_t a3d_object_reserve(uint32_t port_id, uint32_t *object_id) {
    if (!object_id) return A3D_INVALID_PARAMETER;
    *object_id = UINT32_MAX;
    Port *p = port_get(port_id);
    if (!p) return A3D_INVALID_PORT;
    pthread_mutex_lock(&p->lock);
    Object *slot = NULL;
    for (uint32_t i = 0; i < p->object_capacity && !slot; ++i) if (!p->objects[i].used) slot = &p->objects[i];
    if (!slot) { pthread_mutex_unlock(&p->lock); return A3D_OUT_OF_RESOURCES; }
    do ++p->next_object_id; while (!p->next_object_id || p->next_object_id == UINT32_MAX || object_get(p, p->next_object_id));
    memset(slot, 0, sizeof(*slot));
    slot->used = 1;
    slot->id = p->next_object_id;
    *object_id = slot->id;
    pthread_mutex_unlock(&p->lock);
    return 0;
}

static void object_reset(Object *o) {
    queue_clear(&o->pcm);
    o->gain = 0.0f;
    o->has_position = o->passthrough = 0;
}

static int32_t set_attribute(Port *p, Object *o, uint32_t id, const void *value, uint64_t size) {
    switch (id) {
    case ATTR_PCM:
        if (size < sizeof(Pcm) || !value) return A3D_INVALID_PARAMETER;
        return enqueue(&o->pcm, value, 1, p->params.granularity, p->params.queue_depth);
    case ATTR_GAIN:
        if (size >= sizeof(float) && value) memcpy(&o->gain, value, sizeof(float));
        return 0;
    case ATTR_POSITION:
        if (size >= 3 * sizeof(float) && value) { memcpy(o->position, value, 3 * sizeof(float)); o->has_position = 1; }
        return 0;
    case ATTR_PASSTHROUGH:
        if (size >= 4 && value) { uint32_t v; memcpy(&v, value, 4); o->passthrough = v != 0; }
        return 0;
    case ATTR_RESET_STATE:
        object_reset(o);
        return 0;
    default:
        return 0; /* spread, priority, ambisonics, routes: accepted, not rendered */
    }
}

static ABI int32_t a3d_object_set_attributes(uint32_t port_id, uint32_t object_id, uint64_t count, const Attribute *attrs) {
    Port *p = port_get(port_id);
    if (!p) return A3D_INVALID_PORT;
    if (!count || !attrs) return A3D_INVALID_PARAMETER;
    pthread_mutex_lock(&p->lock);
    Object *o = object_get(p, object_id);
    int32_t result = 0;
    if (!o) goto done; /* unreserved meanwhile: ignored, as shadPS4 does */
    if (o->unreserved) { result = A3D_INVALID_OBJECT; goto done; }
    for (uint64_t i = 0; i < count; ++i)
        if (attrs[i].id == ATTR_RESET_STATE) { object_reset(o); break; }
    for (uint64_t i = 0; i < count && !result; ++i)
        if (attrs[i].id != ATTR_RESET_STATE) result = set_attribute(p, o, attrs[i].id, attrs[i].value, attrs[i].size);
done:
    pthread_mutex_unlock(&p->lock);
    return result;
}

static ABI int32_t a3d_object_set_attribute(uint32_t port_id, uint32_t object_id, uint32_t id, const void *value, uint64_t size) {
    Attribute a = {.id = id, .value = value, .size = size};
    return a3d_object_set_attributes(port_id, object_id, 1, &a);
}

static ABI int32_t a3d_object_unreserve(uint32_t port_id, uint32_t object_id) {
    Port *p = port_get(port_id);
    if (!p) return A3D_INVALID_PORT;
    pthread_mutex_lock(&p->lock);
    Object *o = object_get(p, object_id);
    int32_t result = 0;
    if (!o || o->unreserved) result = A3D_INVALID_OBJECT;
    else if (o->pcm.count) o->unreserved = 1; /* its queued sound still plays */
    else o->used = 0;
    pthread_mutex_unlock(&p->lock);
    return result;
}

static ABI int32_t a3d_bed_write2(uint32_t port_id, uint32_t channels, uint32_t format, const void *buffer, uint32_t samples,
                                  uint32_t route, int32_t restricted) {
    (void)route; (void)restricted;
    Port *p = port_get(port_id);
    if (!p) return A3D_INVALID_PORT;
    if (!channels || channels > 8 || format > FORMAT_FLOAT) return A3D_INVALID_PARAMETER;
    Pcm pcm = {.format = format, .buffer = buffer, .num_samples = samples};
    pthread_mutex_lock(&p->lock);
    int32_t result = enqueue(&p->bed, &pcm, channels, p->params.granularity, p->params.queue_depth);
    pthread_mutex_unlock(&p->lock);
    return result;
}

static ABI int32_t a3d_bed_write(uint32_t port_id, uint32_t channels, uint32_t format, const void *buffer, uint32_t samples) {
    return a3d_bed_write2(port_id, channels, format, buffer, samples, 0, 0);
}

static ABI int32_t a3d_port_advance(uint32_t port_id) {
    Port *p = port_get(port_id);
    if (!p) return A3D_INVALID_PORT;
    if (p->params.buffer_mode == MODE_NO_ADVANCE) return A3D_NOT_SUPPORTED;
    pthread_mutex_lock(&p->lock);
    int32_t result = p->mixed.count >= p->params.queue_depth ? A3D_NOT_READY : mix(p);
    pthread_mutex_unlock(&p->lock);
    return result;
}

static int32_t push_one(Port *p, int *pushed) {
    pthread_mutex_lock(&p->lock);
    if (!p->mixed.count) { pthread_mutex_unlock(&p->lock); *pushed = 0; return 0; }
    Granule g = queue_pop(&p->mixed);
    int32_t handle = p->audio_out;
    pthread_mutex_unlock(&p->lock);
    int32_t r = runtime_audio_output_port(handle, g.data); /* paced: blocks until the device needs it */
    free(g.data);
    *pushed = r >= 0;
    if (r >= 0) ++granules_pushed;
    return r < 0 ? r : 0;
}

static ABI int32_t a3d_port_push(uint32_t port_id, uint32_t blocking) {
    Port *p = port_get(port_id);
    if (!p) return A3D_INVALID_PORT;
    if (p->params.buffer_mode != MODE_ADVANCE_AND_PUSH) return A3D_NOT_SUPPORTED;
    if (p->audio_out < 0) {
        int32_t h = runtime_audio_open_port(AUDIO_OUT_TYPE_AUDIO3D, p->params.granularity, AUDIO_OUT_S16_STEREO);
        if (h < 0) { fprintf(stderr, "Runtime: Audio3d: AudioOut port failed (0x%x)\n", (unsigned)h); return h; }
        p->audio_out = h;
    }
    pthread_mutex_lock(&p->lock);
    int full = p->mixed.count >= p->params.queue_depth;
    pthread_mutex_unlock(&p->lock);
    if (!full) return 0;
    int pushed = 0;
    int32_t r = push_one(p, &pushed);
    if (r < 0 || !pushed || blocking == 0) return r;
    for (;;) {
        pthread_mutex_lock(&p->lock);
        full = p->mixed.count >= p->params.queue_depth;
        pthread_mutex_unlock(&p->lock);
        if (!full) break;
        if ((r = push_one(p, &pushed)) < 0 || !pushed) break;
    }
    return r;
}

static ABI int32_t a3d_queue_level(uint32_t port_id, uint32_t *level, uint32_t *available) {
    Port *p = port_get(port_id);
    if (!p) return A3D_INVALID_PORT;
    if (!level && !available) return A3D_INVALID_PARAMETER;
    pthread_mutex_lock(&p->lock);
    uint32_t n = p->mixed.count, depth = p->params.queue_depth;
    pthread_mutex_unlock(&p->lock);
    if (level) *level = n;
    if (available) *available = n < depth ? depth - n : 0;
    return 0;
}

static ABI int32_t a3d_port_set_attribute(uint32_t port_id, uint32_t id, const void *value, uint64_t size) {
    (void)id; (void)size;
    if (!port_get(port_id)) return A3D_INVALID_PORT;
    return value ? 0 : A3D_INVALID_PARAMETER;
}

static ABI int32_t a3d_port_flush(uint32_t port_id) {
    Port *p = port_get(port_id);
    if (!p) return A3D_INVALID_PORT;
    int pushed = 1;
    while (pushed) if (push_one(p, &pushed) < 0) break;
    return 0;
}

static const RuntimeExport exports[] = {
    {"sceAudio3dInitialize", a3d_initialize}, {"sceAudio3dTerminate", a3d_terminate},
    {"sceAudio3dGetDefaultOpenParameters", a3d_default_parameters},
    {"sceAudio3dPortOpen", a3d_port_open}, {"sceAudio3dPortClose", a3d_port_close},
    {"sceAudio3dObjectReserve", a3d_object_reserve}, {"sceAudio3dObjectUnreserve", a3d_object_unreserve},
    {"sceAudio3dObjectSetAttributes", a3d_object_set_attributes}, {"sceAudio3dObjectSetAttribute", a3d_object_set_attribute},
    {"sceAudio3dBedWrite", a3d_bed_write}, {"sceAudio3dBedWrite2", a3d_bed_write2},
    {"sceAudio3dPortAdvance", a3d_port_advance}, {"sceAudio3dPortPush", a3d_port_push},
    {"sceAudio3dPortGetQueueLevel", a3d_queue_level}, {"sceAudio3dPortSetAttribute", a3d_port_set_attribute},
    {"sceAudio3dPortFlush", a3d_port_flush},
};

uintptr_t runtime_audio3d_resolve(const char *name) { return RUNTIME_LOOKUP(exports, name); }

void runtime_audio3d_report(void) {
    printf("Runtime: Audio3d granules mixed=%llu, pushed=%llu\n", granules_mixed, granules_pushed);
}
