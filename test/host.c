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
    // For YV12 also dump the chroma planes so the reference can reproduce
    // the exact input (no RGB->YUV reimplementation needed on the python side).
    if (vi->pixel_type & AVS_CS_YV12) {
        const char *suf[2] = { ".u", ".v" };
        int planes[2] = { AVS_PLANAR_U, AVS_PLANAR_V };
        for (int pl = 0; pl < 2; pl++) {
            char p2[256]; snprintf(p2, sizeof p2, "%s%s", path, suf[pl]);
            FILE *fu = fopen(p2, "wb");
            for (int n = 0; n < n_frames; n++) {
                AVS_VideoFrame *f = avs_get_frame(clip, n);
                const unsigned char *pp = avs_get_read_ptr_p(f, planes[pl]);
                int rw = avs_get_row_size_p(f, planes[pl]);
                int pt = avs_get_pitch_p(f, planes[pl]);
                int hh = avs_get_height_p(f, planes[pl]);
                for (int y = 0; y < hh; y++) fwrite(pp + (size_t)y * pt, 1, rw, fu);
                avs_release_video_frame(f);
            }
            fclose(fu);
        }
    }
    fclose(fp);
    fprintf(stderr, "wrote %s (%d frames, %dx%d)\n", path, n_frames, vi->width, vi->height);
    avs_release_clip(clip);
}

// ColorBars (SMPTE bars), YV12 directly, luma-shifted. Non-uniform content
// — exposes engine/config differences that flat BlankClip hides. Note:
// ColorBars is already a 1-hour static-frame clip, no Loop needed.
static AVS_Value make_cb(int w, int h, int off_y, int len) {
    AVS_Value args[3];
    const char *names[4] = { "width", "height", "pixel_type", NULL };
    args[0] = avs_new_value_int(w);
    args[1] = avs_new_value_int(h);
    args[2] = avs_new_value_string("YV12");
    AVS_Value yuv = avs_invoke(env, "ColorBars", avs_new_value_array(args, 3), names);
    if (avs_is_error(yuv)) die("ColorBars", yuv);
    AVS_Value cy[2];
    const char *n2[3] = { NULL, "off_y", NULL };
    cy[0] = yuv;
    cy[1] = avs_new_value_int(off_y);
    AVS_Value shifted = avs_invoke(env, "ColorYUV", avs_new_value_array(cy, 2), n2);
    avs_release_value(yuv);
    if (avs_is_error(shifted)) die("ColorYUV", shifted);
    // ColorBars is a 1-hour clip by default — keep only `len` frames.
    AVS_Value tr[3] = { shifted, avs_new_value_int(0), avs_new_value_int(len - 1) };
    AVS_Value trimmed = avs_invoke(env, "Trim", avs_new_value_array(tr, 3), NULL);
    avs_release_value(shifted);
    if (avs_is_error(trimmed)) die("Trim", trimmed);
    return trimmed;
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
// Generic named-arg invoker; kinds: 'i'=int 'f'=float 's'=string
static AVS_Value apply_named(AVS_Value src, const char *name, int n_extra,
                             const char **names, const char *kinds, const double *vals, const char **strs) {
    AVS_Value args[16];
    args[0] = src;
    for (int i = 0; i < n_extra; i++) {
        if (kinds[i] == 'i') args[1 + i] = avs_new_value_int((int)vals[i]);
        else if (kinds[i] == 's') args[1 + i] = avs_new_value_string(strs[i]);
        else args[1 + i] = avs_new_value_float(vals[i]);
    }
    return avs_invoke(env, name, avs_new_value_array(args, 1 + n_extra), names);
}

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

    // 19) contrast restore on darkened ColorBars
    {
        AVS_Value yuv = make_cb(64, 48, -110, 3);
        render(yuv, "/mnt/agents/output/hdlt/host_cb_ctr_src.raw");
        AVS_Value args[2];
        const char *nm[] = { NULL, "contrast", NULL };
        args[0] = yuv;
        args[1] = avs_new_value_float(0.8);
        AVS_Value out = avs_invoke(env, "Aurora", avs_new_value_array(args, 2), nm);
        if (avs_is_error(out)) die("Aurora cb_ctr", out);
        render(out, "/mnt/agents/output/hdlt/host_cb_ctr.raw");
        avs_release_value(out); avs_release_value(yuv);
    }

    avs_delete_script_environment(env);
    fprintf(stderr, "ALL OK\n");
    return 0;
}
