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
    // ---- Aurora scenarios (YV12) ----
    // 5) default engine (guided), dark clip
    {
        AVS_Value rgb = make_clip(64, 48, 6, 0x202020);
        AVS_Value yuv = invoke1("ConvertToYV12", rgb);
        avs_release_value(rgb);
        if (avs_is_error(yuv)) die("ConvertToYV12", yuv);
        render(yuv, "/tmp/host_aurora_default_src.raw");
        AVS_Value out = apply_named(yuv, "Aurora", 0, NULL, NULL, NULL, NULL);
        if (avs_is_error(out)) die("Aurora default", out);
        render(out, "/tmp/host_aurora_default.raw");
        avs_release_value(out); avs_release_value(yuv);
    }
    // 6) CLAHE engine
    {
        AVS_Value rgb = make_clip(64, 48, 3, 0x202020);
        AVS_Value yuv = invoke1("ConvertToYV12", rgb);
        avs_release_value(rgb);
        AVS_Value args[2];
        const char *nm[] = { NULL, "engine", NULL };
        const char *kinds = "s";
        const double vl[] = { 0.0 };
        const char *ss[] = { "clahe" };
        args[0] = yuv;
        args[1] = avs_new_value_string("clahe");
        render(yuv, "/tmp/host_aurora_clahe_src.raw");
        AVS_Value out = avs_invoke(env, "Aurora", avs_new_value_array(args, 2), nm);
        if (avs_is_error(out)) die("Aurora clahe", out);
        render(out, "/tmp/host_aurora_clahe.raw");
        avs_release_value(out); avs_release_value(yuv);
    }
    // 7) freezer + corrector + reducer + shift + black_clip
    {
        AVS_Value rgb = make_clip(64, 48, 4, 0x182038);
        AVS_Value yuv = invoke1("ConvertToYV12", rgb);
        avs_release_value(rgb);
        AVS_Value args[6];
        const char *nm[] = { NULL, "freezer", "corrector", "reducer", "black_clip", "shift", NULL };
        args[0] = yuv;
        args[1] = avs_new_value_int(0);
        args[2] = avs_new_value_float(0.9);
        args[3] = avs_new_value_float(1.0);
        args[4] = avs_new_value_float(0.01);
        args[5] = avs_new_value_int(4);
        render(yuv, "/tmp/host_aurora_freeze_src.raw");
        AVS_Value out = avs_invoke(env, "Aurora", avs_new_value_array(args, 6), nm);
        if (avs_is_error(out)) die("Aurora freezer", out);
        render(out, "/tmp/host_aurora_freeze.raw");
        avs_release_value(out); avs_release_value(yuv);
    }

    avs_delete_script_environment(env);
    fprintf(stderr, "ALL OK\n");
    return 0;
}
