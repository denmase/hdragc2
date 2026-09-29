//! AviSynth+ port of the VapourSynth invert example:
//! https://github.com/vapoursynth/vapoursynth/blob/master/sdk/invert_example.c
//!
//! Registers `ZInvert(clip, bool "enabled")` — inverts all planes of planar
//! 8..16-bit integer and 32-bit float formats.

const std = @import("std");
const avs = @import("avisynth");
const c = avs.c;

// https://ziglang.org/documentation/master/#Choosing-an-Allocator
const allocator = std.heap.c_allocator;

/// The loaded C API function table. Set once in avisynth_c_plugin_init,
/// before any filter code can run.
var api: *const avs.AvsApi = undefined;

const Data = struct {
    enabled: bool,
};

fn invertPlane(
    comptime T: type,
    noalias dst_ptr: [*]u8,
    noalias src_ptr: [*]const u8,
    dst_pitch: usize,
    src_pitch: usize,
    row_size: usize,
    height: usize,
    // Result is `minuend - s`, clamped to [0, peak] for integer types.
    // Full-range planes pass minuend == peak; integer chroma passes
    // minuend == peak + 1 (pivot around the midpoint, so neutral stays
    // neutral — same convention as AviSynth+ 3.7.6 Invert() and the
    // zero-centered float chroma negation).
    minuend: if (@typeInfo(T) == .float) T else u32,
    peak: u32,
) void {
    const width = row_size / @sizeOf(T);
    var dst_row = dst_ptr;
    var src_row = src_ptr;
    for (0..height) |_| {
        const dst: [*]T = @ptrCast(@alignCast(dst_row));
        const src: [*]const T = @ptrCast(@alignCast(src_row));
        for (dst[0..width], src[0..width]) |*d, s| {
            // Saturating sub so out-of-range integer input can't underflow.
            d.* = if (@typeInfo(T) == .float) minuend - s else @intCast(@min(minuend -| s, peak));
        }
        dst_row += dst_pitch;
        src_row += src_pitch;
    }
}

fn invertGetFrame(fi: [*c]c.AVS_FilterInfo, n: c_int) callconv(.c) [*c]c.AVS_VideoFrame {
    const d: *Data = @ptrCast(@alignCast(fi.*.user_data));

    const src = api.avs_get_frame.?(fi.*.child, n);
    if (src == null) return null;

    // Pass the source frame (and its reference) straight through.
    if (!d.enabled) return src;

    defer api.avs_release_video_frame.?(src);

    // prop_src=src carries the frame properties over to the new frame (V8+).
    const dst = api.avs_new_video_frame_p_a.?(fi.*.env, &fi.*.vi, src, c.AVS_FRAME_ALIGN);
    if (dst == null) {
        fi.*.@"error" = "ZInvert: could not allocate the destination frame.";
        return null;
    }

    const vi = &fi.*.vi;
    const yuv_planes = [4]c_int{ c.AVS_PLANAR_Y, c.AVS_PLANAR_U, c.AVS_PLANAR_V, c.AVS_PLANAR_A };
    const rgb_planes = [4]c_int{ c.AVS_PLANAR_R, c.AVS_PLANAR_G, c.AVS_PLANAR_B, c.AVS_PLANAR_A };
    const is_rgb = c.avs_is_rgb(vi) != 0;
    const planes = if (is_rgb) rgb_planes else yuv_planes;
    const num_planes: usize = @intCast(api.avs_num_components.?(vi));

    for (planes[0..num_planes]) |plane| {
        const src_p = api.avs_get_read_ptr_p.?(src, plane);
        const dst_p = api.avs_get_write_ptr_p.?(dst, plane);
        const src_pitch: usize = @intCast(api.avs_get_pitch_p.?(src, plane));
        const dst_pitch: usize = @intCast(api.avs_get_pitch_p.?(dst, plane));
        const row_size: usize = @intCast(api.avs_get_row_size_p.?(src, plane));
        const height: usize = @intCast(api.avs_get_height_p.?(src, plane));

        const chroma = !is_rgb and (plane == c.AVS_PLANAR_U or plane == c.AVS_PLANAR_V);

        switch (api.avs_component_size.?(vi)) {
            1 => invertPlane(u8, dst_p, src_p, dst_pitch, src_pitch, row_size, height, if (chroma) 256 else 255, 255),
            2 => {
                const shift: u5 = @intCast(api.avs_bits_per_component.?(vi));
                const peak = (@as(u32, 1) << shift) - 1;
                invertPlane(u16, dst_p, src_p, dst_pitch, src_pitch, row_size, height, if (chroma) peak + 1 else peak, peak);
            },
            // Float chroma is centered on 0.0, so inverting is negation.
            else => invertPlane(f32, dst_p, src_p, dst_pitch, src_pitch, row_size, height, if (chroma) 0.0 else 1.0, 0),
        }
    }

    return dst;
}

fn invertSetCacheHints(fi: [*c]c.AVS_FilterInfo, cachehints: c_int, frame_range: c_int) callconv(.c) c_int {
    _ = fi;
    _ = frame_range;
    return if (cachehints == c.AVS_CACHE_GET_MTMODE) c.AVS_MT_NICE_FILTER else 0;
}

fn invertFree(fi: [*c]c.AVS_FilterInfo) callconv(.c) void {
    const d: *Data = @ptrCast(@alignCast(fi.*.user_data));
    allocator.destroy(d);
    fi.*.user_data = null;
}

fn invertCreate(env: ?*c.AVS_ScriptEnvironment, args: c.AVS_Value, user_data: ?*anyopaque) callconv(.c) c.AVS_Value {
    _ = user_data;

    var fi: [*c]c.AVS_FilterInfo = undefined;
    // store_child=1: fi.child holds the source clip and the env releases it.
    const clip = api.avs_new_c_filter.?(env, &fi, c.avs_array_elt(args, 0), 1);
    // avs_set_to_clip below takes its own reference, so ours is released
    // unconditionally; on the error paths this destroys the half-built filter
    // (safe — get_frame/free_filter/user_data are not set yet).
    defer api.avs_release_clip.?(clip);

    const vi = &fi.*.vi;
    if (c.avs_has_video(vi) == 0)
        return c.avs_new_value_error("ZInvert: input clip must have video.");
    if (c.avs_is_planar(vi) == 0)
        return c.avs_new_value_error("ZInvert: only planar formats are supported.");

    const enabled_arg = c.avs_array_elt(args, 1);
    const enabled = if (c.avs_defined(enabled_arg) != 0) c.avs_as_bool(enabled_arg) != 0 else true;

    const data = allocator.create(Data) catch return c.avs_new_value_error("ZInvert: out of memory.");
    data.* = .{ .enabled = enabled };

    fi.*.user_data = data;
    fi.*.get_frame = &invertGetFrame;
    fi.*.set_cache_hints = &invertSetCacheHints;
    fi.*.free_filter = &invertFree;

    var v: c.AVS_Value = undefined;
    api.avs_set_to_clip.?(&v, clip);
    return v;
}

// V8 is the floor: avs_new_video_frame_p_a and frame properties.
const REQUIRED_INTERFACE_VERSION: c_int = 8;
const REQUIRED_BUGFIX_VERSION: c_int = 0;

// Everything called through `api` above. Loading fails if any is missing
// from the avisynth library; the rest of the table loads as optional.
const required_functions = [_][*:0]const u8{
    "avs_add_function",
    "avs_bits_per_component",
    "avs_component_size",
    "avs_get_frame",
    "avs_get_height_p",
    "avs_get_pitch_p",
    "avs_get_read_ptr_p",
    "avs_get_row_size_p",
    "avs_get_write_ptr_p",
    "avs_new_c_filter",
    "avs_new_video_frame_p_a",
    "avs_num_components",
    "avs_release_clip",
    "avs_release_video_frame",
    "avs_set_to_clip",
};

export fn avisynth_c_plugin_init(env: ?*c.AVS_ScriptEnvironment) callconv(.c) [*:0]const u8 {
    const e = env orelse return "ZInvert: plugin init called without a script environment.";

    api = avs.getApi(e, REQUIRED_INTERFACE_VERSION, REQUIRED_BUGFIX_VERSION, &required_functions) catch
        return avs.getLastError().ptr;

    _ = api.avs_add_function.?(e, "ZInvert", "c[enabled]b", &invertCreate, null);

    return "ZInvert example plugin";
}
