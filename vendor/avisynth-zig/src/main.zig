const std = @import("std");

/// Raw AviSynth+ C API types and constants (avisynth_c.h with AVSC_NO_DECLSPEC,
/// so API functions are exposed as `*_func` pointer typedefs, not extern symbols).
pub const c = @cImport({
    @cDefine("AVSC_NO_DECLSPEC", "1");
    // Skip the header's built-in LoadLibrary-based AVS_Library helpers (they
    // require windows.h); the C++ loader dependency replaces them.
    @cDefine("EXTERNAL_AVS_C_API_LOADER", "1");
    @cInclude("avisynth_c.h");
});

pub const INTERFACE_VERSION: c_int = c.AVISYNTH_INTERFACE_VERSION;
pub const INTERFACE_BUGFIX_VERSION: c_int = c.AVISYNTHPLUS_INTERFACE_BUGFIX_VERSION;

// The loader's function table definition, wired in via build.zig as an
// anonymous import from the avs_loader dependency — the exact file its
// C++ side is compiled with.
const api_functions_inc = @embedFile("avs_c_api_functions.inc");

/// API function names parsed from avs_c_api_functions.inc, in table order.
pub const api_function_names: []const []const u8 = blk: {
    @setEvalBranchQuota(100_000);
    var names: []const []const u8 = &.{};
    var it = std.mem.tokenizeAny(u8, api_functions_inc, "\r\n");
    while (it.next()) |line| {
        const trimmed = std.mem.trim(u8, line, " \t");
        if (!std.mem.startsWith(u8, trimmed, "FUNC(")) continue;
        const name = trimmed[5..std.mem.indexOfScalarPos(u8, trimmed, 5, ')').?];
        names = names ++ .{name};
    }
    break :blk names;
};

/// Mirror of the C++ `avisynth_c_api_pointers` struct built from
/// avs_c_api_functions.inc. Hand-written (not comptime-derived) so ZLS can
/// resolve the fields; field order MUST match the .inc exactly (verified at
/// comptime below — any drift is a compile error). Regenerate with:
///   sed -E 's/^FUNC\((.+)\)$/    \1: c.\1_func,/' ../avisynthplus-c-api-dynamic-loader/src/avs_c_api_functions.inc
///
/// Functions missing from the loaded avisynth library are null. Prefer
/// `getApi(..., required_names)` to require the subset you need.
///
/// Docs summarize AviSynth+ C API (`avisynth_c.h`) and FilterSDK
/// (https://avisynthplus.readthedocs.io/en/latest/avisynthdoc/FilterSDK/C_api.html).
pub const AvsApi = extern struct {
    /// Register a script function/filter. Callback returns `AVS_Value` by value.
    /// `params` is the AviSynth param-type string (e.g. `"c[strength]f"`).
    /// Typically called from `avisynth_c_plugin_init` / `avisynth_c_plugin_init2`.
    avs_add_function: c.avs_add_function_func,

    /// V11 alternative to `avs_add_function`: callback writes result into
    /// `*AVS_Value` (by-ref). Useful for FFI that cannot return structs by value.
    avs_add_function_r: c.avs_add_function_r_func,

    /// Register a shutdown callback run when the script environment is destroyed.
    /// Optional `user_data` is passed back to the callback.
    avs_at_exit: c.avs_at_exit_func,

    /// Line-by-line copy (`BitBlt`): `dst`/`src` row bytes with pitches.
    /// Copies `height` rows of `row_size` bytes each.
    avs_bit_blt: c.avs_bit_blt_func,

    /// Bits per component (sample) for the video format (e.g. 8, 10, 16, 32).
    avs_bits_per_component: c.avs_bits_per_component_func,

    /// Total bits per pixel across all components (packed size in bits).
    avs_bits_per_pixel: c.avs_bits_per_pixel_func,

    /// BMP-style buffer size for the video info (row alignment as in classic BMP).
    avs_bmp_size: c.avs_bmp_size_func,

    /// Byte length of `pixels` samples for this format (accounts for packing).
    avs_bytes_from_pixels: c.avs_bytes_from_pixels_func,

    /// Check that the host interface is at least `version`. Returns 0 on success;
    /// non-zero if the host is older. Prefer `avs_get_env_property` with
    /// `AVS_AEP_INTERFACE_VERSION` / `AVS_AEP_INTERFACE_BUGFIX` when available (V9+).
    avs_check_version: c.avs_check_version_func,

    /// V8: clear all keys from a frame property map (`AVS_Map`).
    avs_clear_map: c.avs_clear_map_func,

    /// Error string from the last clip operation (e.g. after `avs_get_frame`),
    /// or null if none. C-API-only; no C++ equivalent.
    avs_clip_get_error: c.avs_clip_get_error_func,

    /// Bytes per sample component (1 for 8-bit, 2 for 16-bit, 4 for float, …).
    avs_component_size: c.avs_component_size_func,

    /// Shallow-copy a clip (increments refcount). Release with `avs_release_clip`.
    avs_copy_clip: c.avs_copy_clip_func,

    /// V8: copy all frame properties from `src` onto `dst`.
    avs_copy_frame_props: c.avs_copy_frame_props_func,

    /// Deep-copy an `AVS_Value` (refcount++, deep-copy dynamic arrays). Result
    /// must be released with `avs_release_value`. Prefer over field-wise copy.
    avs_copy_value: c.avs_copy_value_func,

    /// Shallow-copy a video frame (increments refcount). Release with
    /// `avs_release_video_frame`.
    avs_copy_video_frame: c.avs_copy_video_frame_func,

    /// Create a new script environment (use AviSynth as a library without a
    /// host script). `version` is the requested interface version.
    avs_create_script_environment: c.avs_create_script_environment_func,

    /// Destroy a script environment from `avs_create_script_environment`.
    /// Runs `avs_at_exit` callbacks. Interface V6+.
    avs_delete_script_environment: c.avs_delete_script_environment_func,

    /// True (non-zero) if a script function/filter named `name` is registered.
    avs_function_exists: c.avs_function_exists_func,

    /// V11: element `index` of an array `AVS_Value` (or the value itself if not
    /// an array). Does not bump refcounts; do not release the element alone.
    avs_get_array_elt: c.avs_get_array_elt_func,

    /// V11: number of elements if `v` is an array; otherwise 1.
    avs_get_array_size: c.avs_get_array_size_func,

    /// V11: underlying array pointer of an array `AVS_Value`.
    avs_get_as_array: c.avs_get_as_array_func,

    /// V11: boolean payload of an `AVS_Value` (`'b'`).
    avs_get_as_bool: c.avs_get_as_bool_func,

    /// V11: extract a clip from an `AVS_Value` (like `avs_take_clip`). Increases
    /// clip refcount; release with `avs_release_clip`.
    avs_get_as_clip: c.avs_get_as_clip_func,

    /// V11: error message string if `v` is an error value (`'e'`), else null.
    avs_get_as_error: c.avs_get_as_error_func,

    /// V11: numeric value as double (accepts int/long/float/double types).
    avs_get_as_float: c.avs_get_as_float_func,

    /// V11: integer payload; 64-bit long is truncated to `int`.
    avs_get_as_int: c.avs_get_as_int_func,

    /// V11: full 64-bit integer (`'l'` or promoted `'i'`).
    avs_get_as_long: c.avs_get_as_long_func,

    /// V11: string payload for string or error values; else null.
    avs_get_as_string: c.avs_get_as_string_func,

    /// Read `count` audio samples starting at `start` into `buf`.
    /// `start`/`count` are in samples (not bytes).
    avs_get_audio: c.avs_get_audio_func,

    /// V10: WAVE channel mask stored in video info (when known).
    avs_get_channel_mask: c.avs_get_channel_mask_func,

    /// CPU instruction-set flags (`AVS_CPUF_*` / `AVS_CPU_*`). 32-bit mask;
    /// for flags beyond 32 bits use host V12 `avs_get_cpu_flags_ex` if loaded.
    avs_get_cpu_flags: c.avs_get_cpu_flags_func,

    /// V8: query environment/system property (`AVS_AEP_*`: interface version,
    /// CPU counts, frame align, L2 cache size, …).
    avs_get_env_property: c.avs_get_env_property_func,

    /// Last environment error string, or null if none. Interface V6+.
    avs_get_error: c.avs_get_error_func,

    /// Request frame `n` from a clip. Result must be released with
    /// `avs_release_video_frame`. On failure check `avs_clip_get_error`.
    avs_get_frame: c.avs_get_frame_func,

    /// V8: read-only frame property map for `frame`.
    avs_get_frame_props_ro: c.avs_get_frame_props_ro_func,

    /// V8: read/write frame property map. Map is writable after
    /// `avs_new_video_frame*`, subframe, `avs_make_writable`, or
    /// `avs_make_property_writable`.
    avs_get_frame_props_rw: c.avs_get_frame_props_rw_func,

    /// Height in pixels of plane `plane` (`AVS_PLANAR_*` / `AVS_DEFAULT_PLANE`).
    avs_get_height_p: c.avs_get_height_p_func,

    /// Field parity for frame/field `n`: non-zero if top-field-first /
    /// top field depending on field-based mode.
    avs_get_parity: c.avs_get_parity_func,

    /// Row pitch (stride) in bytes of plane `plane`. May exceed row size due
    /// to alignment; can differ between frames.
    avs_get_pitch_p: c.avs_get_pitch_p_func,

    /// Log2 vertical chroma/plane subsampling for `plane` (0 = full height).
    avs_get_plane_height_subsampling: c.avs_get_plane_height_subsampling_func,

    /// Log2 horizontal chroma/plane subsampling for `plane` (0 = full width).
    avs_get_plane_width_subsampling: c.avs_get_plane_width_subsampling_func,

    /// Read-only pointer to plane `plane` pixel data.
    avs_get_read_ptr_p: c.avs_get_read_ptr_p_func,

    /// Used width of plane `plane` in bytes (not pixels); may be less than pitch
    /// after crop.
    avs_get_row_size_p: c.avs_get_row_size_p_func,

    /// Get script variable `name`. Throws/sets error if missing. Result must be
    /// released with `avs_release_value`. Prefer typed `avs_get_var_*` / try
    /// variants when available.
    avs_get_var: c.avs_get_var_func,

    /// V8: bool variable or `def` if missing/wrong type.
    avs_get_var_bool: c.avs_get_var_bool_func,

    /// V8: double/float variable or `def` if missing/wrong type.
    avs_get_var_double: c.avs_get_var_double_func,

    /// V8: int variable or `def` if missing/wrong type.
    avs_get_var_int: c.avs_get_var_int_func,

    /// V8: int64 variable or `def` if missing/wrong type.
    avs_get_var_long: c.avs_get_var_long_func,

    /// V8: string variable or `def` if missing/wrong type.
    avs_get_var_string: c.avs_get_var_string_func,

    /// V8: try-get variable into `*val`. Returns non-zero on success (then
    /// release `*val`); on failure leaves `*val` untouched.
    avs_get_var_try: c.avs_get_var_try_func,

    /// Clip interface version of this clip instance (not the host interface
    /// version). For host version use `avs_check_version` / `avs_get_env_property`.
    avs_get_version: c.avs_get_version_func,

    /// Pointer to the clip's `AVS_VideoInfo` (format, size, fps, audio, …).
    /// Owned by the clip; valid while the clip is alive.
    avs_get_video_info: c.avs_get_video_info_func,

    /// Writable pointer to plane `plane`. Null if the frame is not writable;
    /// use `avs_make_writable` first when needed.
    avs_get_write_ptr_p: c.avs_get_write_ptr_p_func,

    /// Invoke a script function/filter by `name` with `args` and optional
    /// `arg_names`. Returned `AVS_Value` must be released with `avs_release_value`.
    avs_invoke: c.avs_invoke_func,

    /// True if video info is planar 4:2:0 (any bit depth). Prefer over
    /// `avs_is_yv12` / `avs_is_yuv420p16` / `avs_is_yuv420ps`.
    avs_is_420: c.avs_is_420_func,

    /// True if video info is planar 4:2:2 (any bit depth). Prefer over
    /// `avs_is_yv16` / `avs_is_yuv422p16` / `avs_is_yuv422ps`.
    avs_is_422: c.avs_is_422_func,

    /// True if video info is planar 4:4:4 (any bit depth). Prefer over
    /// `avs_is_yv24` / `avs_is_yuv444p16` / `avs_is_yuv444ps`.
    avs_is_444: c.avs_is_444_func,

    /// V10: true if a WAVE channel mask is set on the video info.
    avs_is_channel_mask_known: c.avs_is_channel_mask_known_func,

    /// True if `pixel_type` matches colorspace flag/mask `c_space`.
    avs_is_color_space: c.avs_is_color_space_func,

    /// True if planar RGB (no alpha).
    avs_is_planar_rgb: c.avs_is_planar_rgb_func,

    /// True if planar RGBA (with alpha plane).
    avs_is_planar_rgba: c.avs_is_planar_rgba_func,

    /// V9: true if the frame's property map is uniquely owned and writable.
    avs_is_property_writable: c.avs_is_property_writable_func,

    /// True if packed RGB48 (16-bit per component, 3 components).
    avs_is_rgb48: c.avs_is_rgb48_func,

    /// True if packed RGB64 (16-bit per component, 4 components).
    avs_is_rgb64: c.avs_is_rgb64_func,

    /// True if the frame buffer is writable (exactly one owner). Writable
    /// frames may be written via `avs_get_write_ptr_p`.
    avs_is_writable: c.avs_is_writable_func,

    /// True if greyscale Y (any bit depth). Prefer over `avs_is_y8` /
    /// `avs_is_y16` / `avs_is_y32`.
    avs_is_y: c.avs_is_y_func,

    /// Deprecated: Y 16-bit only. Prefer `avs_is_y`.
    avs_is_y16: c.avs_is_y16_func,

    /// Deprecated: Y 32-bit float only. Prefer `avs_is_y`.
    avs_is_y32: c.avs_is_y32_func,

    /// Deprecated: Y 8-bit only. Prefer `avs_is_y`.
    avs_is_y8: c.avs_is_y8_func,

    /// Deprecated: YUV420 16-bit only. Prefer `avs_is_420`.
    avs_is_yuv420p16: c.avs_is_yuv420p16_func,

    /// Deprecated: YUV420 32-bit float only. Prefer `avs_is_420`.
    avs_is_yuv420ps: c.avs_is_yuv420ps_func,

    /// Deprecated: YUV422 16-bit only. Prefer `avs_is_422`.
    avs_is_yuv422p16: c.avs_is_yuv422p16_func,

    /// Deprecated: YUV422 32-bit float only. Prefer `avs_is_422`.
    avs_is_yuv422ps: c.avs_is_yuv422ps_func,

    /// Deprecated: YUV444 16-bit only. Prefer `avs_is_444`.
    avs_is_yuv444p16: c.avs_is_yuv444p16_func,

    /// Deprecated: YUV444 32-bit float only. Prefer `avs_is_444`.
    avs_is_yuv444ps: c.avs_is_yuv444ps_func,

    /// True if YUV with alpha (YUVA planar family).
    avs_is_yuva: c.avs_is_yuva_func,

    /// True if classic YV12 (8-bit 4:2:0). For generic 4:2:0 prefer `avs_is_420`.
    avs_is_yv12: c.avs_is_yv12_func,

    /// True if classic YV16 (8-bit 4:2:2). For generic 4:2:2 prefer `avs_is_422`.
    avs_is_yv16: c.avs_is_yv16_func,

    /// True if classic YV24 (8-bit 4:4:4). For generic 4:4:4 prefer `avs_is_444`.
    avs_is_yv24: c.avs_is_yv24_func,

    /// True if YUV 4:1:1 (YV411).
    avs_is_yv411: c.avs_is_yv411_func,

    /// V9: make frame properties writable without copying pixel buffers
    /// (cheap re-reference). Prefer when only props change.
    avs_make_property_writable: c.avs_make_property_writable_func,

    /// Ensure `*pvf` is a uniquely owned writable frame (may copy pixels).
    /// Also makes properties writable.
    avs_make_writable: c.avs_make_writable_func,

    /// Create a C filter clip from an APPLYFUNC callback context. Sets `*fi`
    /// to the filter info (assign `get_frame`, `free_filter`, …). If
    /// `store_child` is true, child lifetime is managed for you.
    avs_new_c_filter: c.avs_new_c_filter_func,

    /// Allocate a new video frame for `vi` with row `align` (classic default 16;
    /// AviSynth+ enforces a minimum if too small). Partially deprecated: prefer
    /// `avs_new_video_frame_p_a` when frame props should be copied. Release with
    /// `avs_release_video_frame`.
    avs_new_video_frame_a: c.avs_new_video_frame_a_func,

    /// V8: allocate a new frame like `NewVideoFrame`, copying frame properties
    /// from `prop_src`. Fixed crash in interface 9.1. Default alignment.
    avs_new_video_frame_p: c.avs_new_video_frame_p_func,

    /// V8: like `avs_new_video_frame_p` with explicit `align`. Preferred for
    /// filters that produce new frames while preserving props.
    avs_new_video_frame_p_a: c.avs_new_video_frame_p_a_func,

    /// Number of color components/planes contribution (1/3/4 depending on format).
    avs_num_components: c.avs_num_components_func,

    /// V8: allocate `nBytes` from the env buffer pool (`AVS_ALLOCTYPE_*`:
    /// normal or pooled). Free with `avs_pool_free`.
    avs_pool_allocate: c.avs_pool_allocate_func,

    /// V8: free a pointer from `avs_pool_allocate`.
    avs_pool_free: c.avs_pool_free_func,

    /// V8: remove property `key` from a map. Returns non-zero on success.
    avs_prop_delete_key: c.avs_prop_delete_key_func,

    /// V8: get clip property at `index`. Optional `*error` uses
    /// `AVS_GETPROPERROR_*`. Caller owns returned clip ref.
    avs_prop_get_clip: c.avs_prop_get_clip_func,

    /// V8: get data/string buffer for property `key` at `index`. Behaviour
    /// fixed in interface 9.1 to match C++ `propGetData`.
    avs_prop_get_data: c.avs_prop_get_data_func,

    /// V8: size of data property in bytes (string length without terminator).
    avs_prop_get_data_size: c.avs_prop_get_data_size_func,

    /// V11: data type hint (`AVS_PROPDATATYPEHINT_*`: unknown/binary/utf8).
    avs_prop_get_data_type_hint: c.avs_prop_get_data_type_hint_func,

    /// V8: get double property at `index`. Optional `*error` is
    /// `AVS_GETPROPERROR_*`.
    avs_prop_get_float: c.avs_prop_get_float_func,

    /// V8: pointer to entire float/double array for `key`.
    avs_prop_get_float_array: c.avs_prop_get_float_array_func,

    /// V11: property as 32-bit float with saturation (clamped double).
    avs_prop_get_float_saturated: c.avs_prop_get_float_saturated_func,

    /// V8: get frame property at `index`. Optional `*error` is
    /// `AVS_GETPROPERROR_*`.
    avs_prop_get_frame: c.avs_prop_get_frame_func,

    /// V8: get int64 property at `index`. Optional `*error` is
    /// `AVS_GETPROPERROR_*`.
    avs_prop_get_int: c.avs_prop_get_int_func,

    /// V8: pointer to entire int64 array for `key`.
    avs_prop_get_int_array: c.avs_prop_get_int_array_func,

    /// V11: property as 32-bit int with saturation (clamped int64).
    avs_prop_get_int_saturated: c.avs_prop_get_int_saturated_func,

    /// V8: property key name at map index `index` (`0 .. num_keys-1`).
    avs_prop_get_key: c.avs_prop_get_key_func,

    /// V8: property type char (`AVS_PROPTYPE_*`: `'i'/'f'/'s'/'c'/'v'/'u'`).
    avs_prop_get_type: c.avs_prop_get_type_func,

    /// V8: number of elements stored under property `key`.
    avs_prop_num_elements: c.avs_prop_num_elements_func,

    /// V8: number of keys in the property map.
    avs_prop_num_keys: c.avs_prop_num_keys_func,

    /// V8: set clip property. `append` is `AVS_PROPAPPENDMODE_*`
    /// (replace/append/touch).
    avs_prop_set_clip: c.avs_prop_set_clip_func,

    /// V8: set data/string property (`length` -1 = strlen). Prefer
    /// `avs_prop_set_data_h` (V11) to attach a type hint.
    avs_prop_set_data: c.avs_prop_set_data_func,

    /// V11: set data/string with type hint (`AVS_PROPDATATYPEHINT_*`).
    avs_prop_set_data_h: c.avs_prop_set_data_h_func,

    /// V8: set double property. `append` is `AVS_PROPAPPENDMODE_*`.
    avs_prop_set_float: c.avs_prop_set_float_func,

    /// V8: replace key with a double array of `size` elements.
    avs_prop_set_float_array: c.avs_prop_set_float_array_func,

    /// V8: set video-frame property. `append` is `AVS_PROPAPPENDMODE_*`.
    avs_prop_set_frame: c.avs_prop_set_frame_func,

    /// V8: set int64 property. `append` is `AVS_PROPAPPENDMODE_*`.
    avs_prop_set_int: c.avs_prop_set_int_func,

    /// V8: replace key with an int64 array of `size` elements.
    avs_prop_set_int_array: c.avs_prop_set_int_array_func,

    /// Decrement clip refcount; destroy when it hits zero (runs filter
    /// `free_filter` if any).
    avs_release_clip: c.avs_release_clip_func,

    /// Release an `AVS_Value` (clip/frame refs, dynamic arrays, 32-bit long/
    /// double storage). Do not call on a C-stack array container itself.
    avs_release_value: c.avs_release_value_func,

    /// Decrement video-frame refcount; free when zero. Required for frames from
    /// get_frame / new_video_frame* / copy / subframe* (unless returned to host).
    avs_release_video_frame: c.avs_release_video_frame_func,

    /// Row size in bytes for plane from `AVS_VideoInfo` (format geometry, not a
    /// live frame).
    avs_row_size: c.avs_row_size_func,

    /// Copy string into environment-owned permanent storage (lives until env
    /// destroy). Use for strings returned into the script graph. `length` -1 =
    /// use strlen. Do not overwrite returned buffer (post-3.7.3 caching).
    avs_save_string: c.avs_save_string_func,

    /// Filter cache / MT hints (`AVS_CACHE_*`, `AVS_MT_*`). Called by the core
    /// on the clip; plugins implement via `AVS_FilterInfo.set_cache_hints`.
    avs_set_cache_hints: c.avs_set_cache_hints_func,

    /// V10: set or clear WAVE channel mask on video info.
    avs_set_channel_mask: c.avs_set_channel_mask_func,

    /// Create/update a global-scope script variable.
    avs_set_global_var: c.avs_set_global_var_func,

    /// Cap frame-buffer cache size (MB). Does not limit all allocations.
    avs_set_memory_max: c.avs_set_memory_max_func,

    /// V11: store a deep-copied dynamic array of `size` values into `*dest`.
    /// Requires `avs_release_value` on the result.
    avs_set_to_array: c.avs_set_to_array_func,

    /// V11: set `*dest` to a boolean `AVS_Value`.
    avs_set_to_bool: c.avs_set_to_bool_func,

    /// Stuff a clip into an `AVS_Value` (clip refcount++). Original clip may
    /// then be released with `avs_release_clip`.
    avs_set_to_clip: c.avs_set_to_clip_func,

    /// V11: set `*dest` to a double. Requires `avs_release_value` (especially
    /// on 32-bit hosts where storage is allocated).
    avs_set_to_double: c.avs_set_to_double_func,

    /// V11: set `*dest` to an error string value (`'e'`).
    avs_set_to_error: c.avs_set_to_error_func,

    /// V11: set `*dest` to a 32-bit float value.
    avs_set_to_float: c.avs_set_to_float_func,

    /// V11: set `*dest` to a 32-bit int value.
    avs_set_to_int: c.avs_set_to_int_func,

    /// V11: set `*dest` to a 64-bit long. Requires `avs_release_value`
    /// (especially on 32-bit hosts).
    avs_set_to_long: c.avs_set_to_long_func,

    /// V11: set `*dest` to a string value (pointer stored; use
    /// `avs_save_string` for ephemeral buffers).
    avs_set_to_string: c.avs_set_to_string_func,

    /// V11: set `*dest` to void (`avs_void` / undefined).
    avs_set_to_void: c.avs_set_to_void_func,

    /// Create/update a local-scope script variable. Returns non-zero if newly
    /// created, zero if an existing value was updated.
    avs_set_var: c.avs_set_var_func,

    /// Set the process working directory used by AviSynth path resolution.
    avs_set_working_dir: c.avs_set_working_dir_func,

    /// printf-style format into environment-owned string storage (same lifetime
    /// rules as `avs_save_string`). ~4096 char limit.
    avs_sprintf: c.avs_sprintf_func,

    /// Sub-window into an interleaved frame (offset/pitch/row_size/height).
    /// Planar formats should use `avs_subframe_planar` / `_a`. Release result.
    avs_subframe: c.avs_subframe_func,

    /// Sub-window into a planar frame (Y/U/V offsets and UV pitch). Release
    /// result with `avs_release_video_frame`. Interface V6+.
    avs_subframe_planar: c.avs_subframe_planar_func,

    /// V8: like `avs_subframe_planar` with alpha-plane offset. Release result.
    avs_subframe_planar_a: c.avs_subframe_planar_a_func,

    /// Extract a clip from an `AVS_Value` (refcount++). Reverse of
    /// `avs_set_to_clip`. Release with `avs_release_clip`; value may then be
    /// released with `avs_release_value`.
    avs_take_clip: c.avs_take_clip_func,

    /// V11 API type-test: true if value is not void (`'v'`).
    avs_val_defined: c.avs_val_defined_func,

    /// V11: true if type is array (`'a'`).
    avs_val_is_array: c.avs_val_is_array_func,

    /// V11: true if type is bool (`'b'`).
    avs_val_is_bool: c.avs_val_is_bool_func,

    /// V11: true if type is clip (`'c'`).
    avs_val_is_clip: c.avs_val_is_clip_func,

    /// V11: true if type is error (`'e'`).
    avs_val_is_error: c.avs_val_is_error_func,

    /// V11: true if value is numeric float-compatible (`'f'/'d'/'i'/'l'`).
    avs_val_is_float: c.avs_val_is_float_func,

    /// V11: true only for strict 32-bit float (`'f'`), not double/int/long.
    avs_val_is_floatf_strict: c.avs_val_is_floatf_strict_func,

    /// V11: true for 32-bit int or 64-bit long (`'i'` or `'l'`).
    avs_val_is_int: c.avs_val_is_int_func,

    /// V11: true only for strict 64-bit long (`'l'`).
    avs_val_is_long_strict: c.avs_val_is_long_strict_func,

    /// V11: true if type is string (`'s'`).
    avs_val_is_string: c.avs_val_is_string_func,

    /// V10: override frame `pixel_type` metadata (special cases only). Frame
    /// should be writable (`avs_make_writable`) first.
    avs_video_frame_amend_pixel_type: c.avs_video_frame_amend_pixel_type_func,

    /// V10: exact pixel type stored on the frame (reliable for frames from
    /// propGetFrame). Auto-maintained by new/make_writable/subframe APIs.
    avs_video_frame_get_pixel_type: c.avs_video_frame_get_pixel_type_func,

    /// vprintf-style format into environment-owned string storage (see
    /// `avs_sprintf` / `avs_save_string`).
    avs_vsprintf: c.avs_vsprintf_func,
};

// Layout guard: AvsApi must list the same functions in the same order as the
// .inc the C++ loader was compiled with, or every call would go through the
// wrong pointer. Any drift is a compile error.
comptime {
    @setEvalBranchQuota(100_000);
    const fields = @typeInfo(AvsApi).@"struct".fields;
    if (fields.len != api_function_names.len) {
        @compileError(std.fmt.comptimePrint(
            "AvsApi has {d} fields but avs_c_api_functions.inc has {d} FUNC entries — regenerate the field list (see AvsApi doc comment)",
            .{ fields.len, api_function_names.len },
        ));
    }
    for (fields, api_function_names) |field, name| {
        if (!std.mem.eql(u8, field.name, name)) {
            @compileError("AvsApi is out of sync with avs_c_api_functions.inc at '" ++ name ++ "' — regenerate the field list (see AvsApi doc comment)");
        }
    }
    // Every entry is a single function pointer; anything else would break the
    // 1:1 layout match with the C++ struct.
    std.debug.assert(@sizeOf(AvsApi) == fields.len * @sizeOf(fields[0].type));
}

extern fn avsz_get_api(
    env: ?*c.AVS_ScriptEnvironment,
    required_interface_version: c_int,
    required_bugfix_version: c_int,
    required_names: ?[*]const [*:0]const u8,
    required_names_count: usize,
) ?*const AvsApi;

extern fn avsz_get_last_error() [*:0]const u8;

/// Loads avisynth (dll/so/dylib) and its C API function table. Call once from
/// `avisynth_c_plugin_init`; the loader does not retry after a failed first
/// load. Functions listed in `required_names` must exist in the loaded
/// library or loading fails; all others are loaded as optional (null when
/// absent). On error, see `getLastError()`.
pub fn getApi(
    env: *c.AVS_ScriptEnvironment,
    required_interface_version: c_int,
    required_bugfix_version: c_int,
    required_names: []const [*:0]const u8,
) error{AvsApiLoadFailed}!*const AvsApi {
    return avsz_get_api(
        env,
        required_interface_version,
        required_bugfix_version,
        if (required_names.len == 0) null else required_names.ptr,
        required_names.len,
    ) orelse error.AvsApiLoadFailed;
}

/// Error description for the last failed `getApi` call.
/// Valid until the next `getApi` call.
pub fn getLastError() [:0]const u8 {
    return std.mem.span(avsz_get_last_error());
}

test "module compiles" {
    std.testing.refAllDecls(@This());
}

test "AvsApi matches the loader's function table" {
    try std.testing.expectEqual(api_function_names.len, @typeInfo(AvsApi).@"struct".fields.len);
    try std.testing.expect(@hasField(AvsApi, "avs_add_function"));
    try std.testing.expect(@hasField(AvsApi, "avs_get_frame"));
    try std.testing.expect(@hasField(AvsApi, "avs_release_video_frame"));
    try std.testing.expect(@hasField(AvsApi, "avs_vsprintf"));
}
