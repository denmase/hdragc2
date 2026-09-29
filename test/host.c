// Minimal host test for the hdragc2 plugin (AviSynth+ C API).
// Renders several HDRAGC scenarios to raw RGB32 files, to be compared
// against the Python reference implementation.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "avisynth_c.h"

#define PLUGIN "/tmp/libhdragc2.so"

static AVS_ScriptEnvironment *env;

static AVS_Value invoke1(const char *name, AVS_Value arg) {
    AVS_Value args[1] = { arg };
    return avs_invoke(env, name, avs_new_value_array(args, 1), NULL);
}

static void die(const char *msg, AVS_Value v) {
    const char *e = avs_is_error(v) ? avs_as_string(v) : avs_get_error(env);
    fprintf(stderr, "FATAL %s (is_error=%d): %s\n", msg, avs_is_error(v), e ? e : "(no error text)");
    exit(1);
}

// render all frames of a clip value to a raw RGB32 file (rows tightened)
static void render(AVS_Value clip_v, const char *path) {
    AVS_Clip *clip = avs_take_clip(clip_v, env);
    FILE *fp = fopen(path, "wb");
    if (!fp) { fprintf(stderr, "cannot open %s\n", path); exit(1); }
    const AVS_VideoInfo *vi = avs_get_video_info(clip);
    int n_frames = vi->num_frames;
    for (int n = 0; n < n_frames; n++) {
        AVS_VideoFrame *f = avs_get_frame(clip, n);
        const char *err = avs_clip_get_error(clip);
        if (err) { fprintf(stderr, "frame %d: %s\n", n, err); exit(1); }
        const unsigned char *p = avs_get_read_ptr_p(f, AVS_PLANAR_Y);
        int row = avs_get_row_size_p(f, AVS_PLANAR_Y);
        int pitch = avs_get_pitch_p(f, AVS_PLANAR_Y);
        int h = avs_get_height_p(f, AVS_PLANAR_Y);
        for (int y = 0; y < h; y++)
            fwrite(p + (size_t)y * pitch, 1, row, fp);
        avs_release_video_frame(f);
    }
    fclose(fp);
    fprintf(stderr, "wrote %s (%d frames, %dx%d)\n", path, n_frames, vi->width, vi->height);
    avs_release_clip(clip);
}

static AVS_Value make_clip(int w, int h, int len, int color) {
    AVS_Value args[6];
    const char *names[7] = { "width", "height", "length", "pixel_type", "color", "fps", NULL };
    args[0] = avs_new_value_int(w);
    args[1] = avs_new_value_int(h);
    args[2] = avs_new_value_int(len);
    args[3] = avs_new_value_string("RGB32");
    args[4] = avs_new_value_int(color);
    args[5] = avs_new_value_int(25);
    AVS_Value v = avs_invoke(env, "BlankClip", avs_new_value_array(args, 6), names);
    if (avs_is_error(v)) die("BlankClip", v);
    return v;
}

// HDRAGC with a list of (name, float value) named args
static AVS_Value apply_hdragc(AVS_Value src, int n_extra, const char **names, const double *vals) {
    AVS_Value args[16];
    args[0] = src;
    for (int i = 0; i < n_extra; i++)
        args[1 + i] = avs_new_value_float(vals[i]);
    AVS_Value v = avs_invoke(env, "HDRAGC", avs_new_value_array(args, 1 + n_extra), names);
    if (avs_is_error(v)) die("HDRAGC", v);
    return v;
}

int main(void) {
    env = avs_create_script_environment(AVISYNTH_INTERFACE_VERSION);
    if (!env) { fprintf(stderr, "no env\n"); return 1; }

    AVS_Value r = invoke1("LoadCPlugin", avs_new_value_string(PLUGIN));
    if (avs_is_error(r)) die("LoadCPlugin", r);
    fprintf(stderr, "LoadCPlugin returned: %s\n", avs_as_string(r));
    fprintf(stderr, "HDRAGC exists: %d\n", avs_function_exists(env, "HDRAGC"));
    avs_release_value(r);

    // 1) identitas: max_gain=1 -> output harus == input persis
    {
        AVS_Value src = make_clip(64, 48, 3, 0x35507A);
        const char *nm[] = { NULL, "max_gain", NULL };
        const double vl[] = { 1.0 };
        AVS_Value out = apply_hdragc(src, 1, nm, vl);
        render(out, "/tmp/host_identity.raw");
        avs_release_value(out); avs_release_value(src);
    }
    // 2) dark frame, default parameters, 8 frames (ring buffer test)
    {
        AVS_Value src = make_clip(64, 48, 8, 0x202020);
        AVS_Value out = apply_hdragc(src, 0, NULL, NULL);
        render(out, "/tmp/host_dark.raw");
        avs_release_value(out); avs_release_value(src);
    }
    // 3) dark, mode 0 (2D pass), circle=5, avg_window=4, response=50
    {
        AVS_Value src = make_clip(48, 32, 6, 0x202020);
        AVS_Value args[5];
        const char *nm[] = { NULL, "mode", "circle", "avg_window", "response", NULL };
        args[0] = src;
        args[1] = avs_new_value_int(0);
        args[2] = avs_new_value_int(5);
        args[3] = avs_new_value_int(4);
        args[4] = avs_new_value_int(50);
        AVS_Value out = avs_invoke(env, "HDRAGC", avs_new_value_array(args, 5), nm);
        if (avs_is_error(out)) die("HDRAGC m0", out);
        render(out, "/tmp/host_dark_m0.raw");
        avs_release_value(out); avs_release_value(src);
    }
    // 4) bright frame (gain should be ~1)
    {
        AVS_Value src = make_clip(64, 48, 3, 0xD0D0D0);
        AVS_Value out = apply_hdragc(src, 0, NULL, NULL);
        render(out, "/tmp/host_bright.raw");
        avs_release_value(out); avs_release_value(src);
    }
    avs_delete_script_environment(env);
    fprintf(stderr, "ALL OK\n");
    return 0;
}
