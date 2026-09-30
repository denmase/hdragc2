const std = @import("std");

pub fn build(b: *std.Build) !void {
    // Default to the GENERIC x86-64 baseline (SSE2 only) so released
    // binaries run on any x86-64 CPU. Native-CPU builds (e.g. on CI
    // runners with AVX-512) crash with "Illegal instruction" on older
    // hardware. Override with -Dcpu=native when benchmarking locally.
    const target = b.standardTargetOptions(.{
        .default_target = .{
            .cpu_model = .{ .explicit = &std.Target.x86.cpu.x86_64 },
        },
    });
    const optimize = b.standardOptimizeOption(.{});

    // ---- modul "avisynth" (vendor avisynth-zig) ----
    const avisynth_mod = b.createModule(.{
        .root_source_file = b.path("vendor/avisynth-zig/src/main.zig"),
        .target = target,
        .optimize = optimize,
        .link_libcpp = true, // loader & shim adalah C++
    });
    avisynth_mod.addIncludePath(b.path("vendor/avisynthplus/avs_core/include"));
    avisynth_mod.addIncludePath(b.path("vendor/avs_loader/src"));
    avisynth_mod.addAnonymousImport("avs_c_api_functions.inc", .{
        .root_source_file = b.path("vendor/avs_loader/src/avs_c_api_functions.inc"),
    });
    const cpp_flags = [_][]const u8{ "-std=c++20", "-fno-c++-static-destructors" };
    avisynth_mod.addCSourceFile(.{ .file = b.path("vendor/avs_loader/src/avs_c_api_loader.cpp"), .flags = &cpp_flags });
    avisynth_mod.addCSourceFile(.{ .file = b.path("vendor/avisynth-zig/src/shim.cpp"), .flags = &cpp_flags });

    // ---- modul plugin ----
    const mod = b.createModule(.{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
    });
    mod.addImport("avisynth", avisynth_mod);

    const lib = b.addLibrary(.{
        .name = "hdragc2",
        .linkage = .dynamic,
        .root_module = mod,
    });
    b.installArtifact(lib);
}
