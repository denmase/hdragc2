const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const loader_dep = b.dependency("avs_loader", .{});
    const avisynth_dep = b.dependency("avisynthplus", .{});

    const mod = b.addModule("avisynth", .{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
        .link_libcpp = true,
    });

    // C API headers (avisynth_c.h + avs/) and the loader's header/impl.
    mod.addIncludePath(avisynth_dep.path("avs_core/include"));
    mod.addIncludePath(loader_dep.path("src"));

    // The loader is C++20; consumers of the module get these compiled in automatically.
    //
    // -fno-c++-static-destructors works around Zig 0.16.0 toolchain: in a
    // DLL with a Zig root module, zig's start.zig provides the entry point
    // instead of mingw's dllcrt2, so the mingw CRT atexit table is never
    // initialized (left at its -1 sentinel). The loader's function-local static
    // singleton has a std::string member, and registering its destructor via
    // __cxa_atexit then crashes with heap corruption (access violation reading
    // 0xFFFFFFFFFFFFFFFF) inside avisynth_c_plugin_init. Skipping static-dtor
    // registration is safe here: the loader singleton must live until unload
    // anyway, and the OS reclaims its memory.
    const cpp_flags = [_][]const u8{ "-std=c++20", "-fno-c++-static-destructors" };
    mod.addCSourceFiles(.{
        .root = loader_dep.path("src"),
        .files = &.{"avs_c_api_loader.cpp"},
        .flags = &cpp_flags,
    });
    mod.addCSourceFile(.{
        .file = b.path("src/shim.cpp"),
        .flags = &cpp_flags,
    });

    // Used to comptime-verify that AvsApi stays in sync with the loader's function table.
    mod.addAnonymousImport("avs_c_api_functions.inc", .{
        .root_source_file = loader_dep.path("src/avs_c_api_functions.inc"),
    });

    const lib = b.addLibrary(.{
        .name = "avisynth",
        .linkage = .static,
        .root_module = mod,
    });
    b.installArtifact(lib);

    const tests = b.addTest(.{ .root_module = mod });
    const run_tests = b.addRunArtifact(tests);
    const test_step = b.step("test", "Run module tests");
    test_step.dependOn(&run_tests.step);
}
