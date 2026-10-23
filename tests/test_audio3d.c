/* libSceAudio3d runtime test: objects with gain and position are mixed to a stereo granule and
 * pushed to the (stubbed) AudioOut port.
 * Build: cc -std=c11 -O2 -pthread -Isrc tests/test_audio3d.c -lm -o out/audio3d-test */
#include "../src/runtime_audio3d.c"
#include <assert.h>

uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *nid) {
    (void)table; (void)count; (void)nid;
    return 0;
}
static int16_t last[512 * 2];
static int outputs;
int32_t runtime_audio_open_port(int32_t type, uint32_t frames, uint32_t format) {
    assert(type == 126 && frames == 256 && format == 1);
    return 0x207e0015;
}
int32_t runtime_audio_output_port(int32_t handle, const void *data) {
    assert(handle == 0x207e0015);
    memcpy(last, data, 256 * 4);
    ++outputs;
    return 512;
}
int32_t runtime_audio_close_port(int32_t handle) { (void)handle; return 0; }

int main(void) {
    assert(a3d_initialize(0) == 0);
    OpenParameters params;
    a3d_default_parameters(&params);
    uint32_t port;
    assert(a3d_port_open(0xff, &params, &port) == 0 && port == 0);
    uint32_t right_obj, silent;
    assert(a3d_object_reserve(port, &right_obj) == 0);
    assert(a3d_object_reserve(port, &silent) == 0 && silent != right_obj);
    static int16_t pcm[256];
    for (int i = 0; i < 256; ++i) pcm[i] = 16384;
    Pcm block = {.format = FORMAT_S16, .buffer = pcm, .num_samples = 256};
    float gain = 1.0f, position[3] = {10.0f, 0.0f, 0.0f}; /* fully to the right */
    Attribute attrs[] = {{ATTR_GAIN, 0, &gain, 4}, {ATTR_POSITION, 0, position, 12}, {ATTR_PCM, 0, &block, sizeof(block)}};
    assert(a3d_object_set_attributes(port, right_obj, 3, attrs) == 0);
    Attribute pcm_only = {ATTR_PCM, 0, &block, sizeof(block)}; /* no gain: silent */
    assert(a3d_object_set_attributes(port, silent, 1, &pcm_only) == 0);
    assert(a3d_port_advance(port) == 0);
    assert(a3d_port_advance(port) == 0);              /* empty queues: a silent granule */
    assert(a3d_port_advance(port) == A3D_NOT_READY);  /* queue depth 2 */
    uint32_t level, available;
    assert(a3d_queue_level(port, &level, &available) == 0 && level == 2 && available == 0);
    /* Push plays granules until the queue has room again: the first one, the mixed object. */
    assert(a3d_port_push(port, 1) == 0);
    assert(outputs == 1);
    assert(last[0] == 0 && last[1] > 20000); /* right only, mono level kept at full pan */
    assert(a3d_queue_level(port, &level, &available) == 0 && level == 1 && available == 1);
    assert(a3d_port_push(port, 1) == 0 && outputs == 1); /* not full: nothing to play yet */
    assert(a3d_object_unreserve(port, right_obj) == 0);
    assert(a3d_object_unreserve(port, right_obj) == A3D_INVALID_OBJECT);
    assert(a3d_port_close(port) == 0);
    puts("audio3d test: OK");
    return 0;
}
