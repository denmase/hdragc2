const std = @import("std");
const zon = @import("build.zig.zon");

pub fn build(b: *std.Build) !void {
    try ensureZigVersion(try .parse(zon.minimum_zig_version));
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const mod = b.createModule(.{
        .root_source_file = b.path("src/invert_example.zig"),
        .target = target,
        .optimize = optimize,
    });

    const lib = b.addLibrary(.{
        .name = "invert_example",
        .linkage = .dynamic,
        .root_module = mod,
    });

    const avisynth_dep = b.dependency("avisynth", .{
        .target = target,
        .optimize = optimize,
    });

    lib.root_module.addImport("avisynth", avisynth_dep.module("avisynth"));
    lib.root_module.link_libc = true;

    if (lib.root_module.optimize == .ReleaseFast) {
        lib.root_module.strip = true;
    }

    b.installArtifact(lib);
}

fn ensureZigVersion(min_zig_version: std.SemanticVersion) !void {
    var installed_ver = @import("builtin").zig_version;
    installed_ver.build = null;

    if (installed_ver.order(min_zig_version) == .lt) {
        std.log.err("\n" ++
            \\---------------------------------------------------------------------------
            \\
            \\Installed Zig compiler version is too old.
            \\
            \\Min. required version: {any}
            \\Installed version: {any}
            \\
            \\Please install newer version and try again.
            \\Latest version can be found here: https://ziglang.org/download/
            \\
            \\---------------------------------------------------------------------------
            \\
        , .{ min_zig_version, installed_ver });
        return error.ZigIsTooOld;
    }
}
