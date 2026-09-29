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
    // ---- Aurora v2 scenarios ----
    // 8) domain="linear" on dark clip
    {
        AVS_Value rgb = make_clip(64, 48, 3, 0x202020);
        AVS_Value yuv = invoke1("ConvertToYV12", rgb);
        avs_release_value(rgb);
        AVS_Value args[2];
        const char *nm[] = { NULL, "domain", NULL };
        args[0] = yuv;
        args[1] = avs_new_value_string("linear");
        render(yuv, "/tmp/host_aurora_lin_src.raw");
        AVS_Value out = avs_invoke(env, "Aurora", avs_new_value_array(args, 2), nm);
        if (avs_is_error(out)) die("Aurora lin", out);
        render(out, "/tmp/host_aurora_lin.raw");
        avs_release_value(out); avs_release_value(yuv);
    }
    // 9) domain="log"
    {
        AVS_Value rgb = make_clip(64, 48, 3, 0x202020);
        AVS_Value yuv = invoke1("ConvertToYV12", rgb);
        avs_release_value(rgb);
        AVS_Value args[2];
        const char *nm[] = { NULL, "domain", NULL };
        args[0] = yuv;
        args[1] = avs_new_value_string("log");
        render(yuv, "/tmp/host_aurora_log_src.raw");
        AVS_Value out = avs_invoke(env, "Aurora", avs_new_value_array(args, 2), nm);
        if (avs_is_error(out)) die("Aurora log", out);
        render(out, "/tmp/host_aurora_log.raw");
        avs_release_value(out); avs_release_value(yuv);
    }
    // 10) YUV444P direct input
    {
        AVS_Value args[6];
        const char *nm[] = { "width", "height", "length", "pixel_type", "color", "fps", NULL };
        args[0] = avs_new_value_int(64);
        args[1] = avs_new_value_int(48);
        args[2] = avs_new_value_int(3);
        args[3] = avs_new_value_string("YV24");  // classic name for YUV444P8
        args[4] = avs_new_value_int(0x202020);
        args[5] = avs_new_value_int(25);
        AVS_Value yuv = avs_invoke(env, "BlankClip", avs_new_value_array(args, 6), nm);
        if (avs_is_error(yuv)) die("BlankClip 444", yuv);
        render(yuv, "/tmp/host_aurora_444_src.raw");
        AVS_Value out = apply_named(yuv, "Aurora", 0, NULL, NULL, NULL, NULL);
        if (avs_is_error(out)) die("Aurora 444", out);
        render(out, "/tmp/host_aurora_444.raw");
        avs_release_value(out); avs_release_value(yuv);
    }
    // 11) temporal pg smoothing + scene cut (dark 3f ++ bright 3f)
    {
        AVS_Value rgbA = make_clip(64, 48, 3, 0x202020);
        AVS_Value yuvA = invoke1("ConvertToYV12", rgbA);
        avs_release_value(rgbA);
        AVS_Value rgbB = make_clip(64, 48, 3, 0xC8C8C8);
        AVS_Value yuvB = invoke1("ConvertToYV12", rgbB);
        avs_release_value(rgbB);
        AVS_Value sp_args[2] = { yuvA, yuvB };
        AVS_Value spliced = avs_invoke(env, "UnalignedSplice", avs_new_value_array(sp_args, 2), NULL);
        if (avs_is_error(spliced)) die("Splice", spliced);
        avs_release_value(yuvA); avs_release_value(yuvB);
        AVS_Value args[3];
        const char *nm[] = { NULL, "pg_smooth", "scene_cut", NULL };
        args[0] = spliced;
        args[1] = avs_new_value_float(0.7);
        args[2] = avs_new_value_float(0.2);
        render(spliced, "/tmp/host_aurora_tpg_src.raw");
        AVS_Value out = avs_invoke(env, "Aurora", avs_new_value_array(args, 3), nm);
        if (avs_is_error(out)) die("Aurora tpg", out);
        render(out, "/tmp/host_aurora_tpg.raw");
        avs_release_value(out); avs_release_value(spliced);
    }
    // ---- end Aurora v2 scenarios ----

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

    // ---- ColorBars scenarios (non-uniform content) ----
    // 12) default engine on darkened ColorBars
    {
        AVS_Value yuv = make_cb(64, 48, -110, 3);
        render(yuv, "/tmp/host_cb_dark_src.raw");
        AVS_Value out = apply_named(yuv, "Aurora", 0, NULL, NULL, NULL, NULL);
        if (avs_is_error(out)) die("Aurora cb_dark", out);
        render(out, "/tmp/host_cb_dark.raw");
        avs_release_value(out); avs_release_value(yuv);
    }
    // 13) CLAHE engine on the same content
    {
        AVS_Value yuv = make_cb(64, 48, -110, 3);
        render(yuv, "/tmp/host_cb_clahe_src.raw");
        AVS_Value args[2];
        const char *nm[] = { NULL, "engine", NULL };
        args[0] = yuv;
        args[1] = avs_new_value_string("clahe");
        AVS_Value out = avs_invoke(env, "Aurora", avs_new_value_array(args, 2), nm);
        if (avs_is_error(out)) die("Aurora cb_clahe", out);
        render(out, "/tmp/host_cb_clahe.raw");
        avs_release_value(out); avs_release_value(yuv);
    }
    // 14) domain=linear on the same content
    {
        AVS_Value yuv = make_cb(64, 48, -110, 3);
        render(yuv, "/tmp/host_cb_lin_src.raw");
        AVS_Value args[2];
        const char *nm[] = { NULL, "domain", NULL };
        args[0] = yuv;
        args[1] = avs_new_value_string("linear");
        AVS_Value out = avs_invoke(env, "Aurora", avs_new_value_array(args, 2), nm);
        if (avs_is_error(out)) die("Aurora cb_lin", out);
        render(out, "/tmp/host_cb_lin.raw");
        avs_release_value(out); avs_release_value(yuv);
    }
    // 15) scene cut: darkened cb (3f) ++ brightened cb (3f), pg smoothing on
    {
        AVS_Value dark = make_cb(64, 48, -110, 3);
        AVS_Value bright = make_cb(64, 48, 90, 3);
        AVS_Value sp_args[2] = { dark, bright };
        AVS_Value spliced = avs_invoke(env, "UnalignedSplice", avs_new_value_array(sp_args, 2), NULL);
        if (avs_is_error(spliced)) die("Splice cb", spliced);
        avs_release_value(dark); avs_release_value(bright);
        AVS_Value args[3];
        const char *nm[] = { NULL, "pg_smooth", "scene_cut", NULL };
        args[0] = spliced;
        args[1] = avs_new_value_float(0.5);
        args[2] = avs_new_value_float(0.3);
        render(spliced, "/tmp/host_cb_tpg_src.raw");  // (already present)
        AVS_Value out = avs_invoke(env, "Aurora", avs_new_value_array(args, 3), nm);
        if (avs_is_error(out)) die("Aurora cb_tpg", out);
        render(out, "/tmp/host_cb_tpg.raw");
        avs_release_value(out); avs_release_value(spliced);
    }

    // 16) protect taper in log domain on BRIGHT content
    {
        AVS_Value yuv = make_cb(64, 48, 90, 3);
        render(yuv, "/tmp/host_cb_prot_src.raw");
        AVS_Value args[3];
        const char *nm[] = { NULL, "domain", "protect", NULL };
        args[0] = yuv;
        args[1] = avs_new_value_string("log");
        args[2] = avs_new_value_int(1);
        AVS_Value out = avs_invoke(env, "Aurora", avs_new_value_array(args, 3), nm);
        if (avs_is_error(out)) die("Aurora cb_prot", out);
        render(out, "/tmp/host_cb_prot.raw");
        avs_release_value(out); avs_release_value(yuv);
    }

    avs_delete_script_environment(env);
    fprintf(stderr, "ALL OK\n");
    return 0;
}
