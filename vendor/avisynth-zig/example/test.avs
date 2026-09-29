# ZInvert example test script.
# Run with test.bat: the per-frame Assert()s below run for EVERY frame while
# the script is loaded (a failed Assert aborts loading, so ffmpeg exits
# nonzero); ffmpeg then decodes the clip and keeps the last frame as
# test_result.png. If the PNG exists, everything passed.
#
# NOTE: the asserts deliberately run at script level (for-loop + setting
# `global current_frame`) instead of inside ScriptClip — ScriptClip only
# paints runtime errors onto the frame and the decode still "succeeds".
#
# Build the plugin first:  zig build -Doptimize=ReleaseFast

LoadPlugin("zig-out/bin/invert_example.dll")

frames = 99
src = ColorBars(width=320, height=240, pixel_type="YV24").KillAudio().Trim(0, frames)

# ---- registration + format preservation ----
Assert(FunctionExists("ZInvert"), "ZInvert is not registered")

inv = src.ZInvert()
Assert(inv.Width() == src.Width(), "ZInvert changed the width")
Assert(inv.Height() == src.Height(), "ZInvert changed the height")
Assert(inv.FrameCount() == src.FrameCount(), "ZInvert changed the frame count")
Assert(inv.FrameRateNumerator() == src.FrameRateNumerator(), "ZInvert changed the fps numerator")
Assert(inv.FrameRateDenominator() == src.FrameRateDenominator(), "ZInvert changed the fps denominator")
Assert(PixelType(inv) == PixelType(src), "ZInvert changed the pixel type")

src10  = src.ConvertBits(10)
src16  = src.ConvertBits(16)
srcf   = src.ConvertBits(32)
srcrgb = src.ConvertToPlanarRGBA()
srcy   = src.ConvertToY()

Assert(BitsPerComponent(src10.ZInvert()) == 10, "ZInvert changed the 10-bit depth")
Assert(BitsPerComponent(src16.ZInvert()) == 16, "ZInvert changed the 16-bit depth")
Assert(BitsPerComponent(srcf.ZInvert()) == 32, "ZInvert changed the float depth")
Assert(srcrgb.ZInvert().IsPlanarRGBA(), "ZInvert changed the planar RGBA type")
Assert(srcy.ZInvert().IsY(), "ZInvert changed the grayscale type")

# ---- clips compared by the per-frame asserts ----
builtin = src.Invert()
pass    = src.ZInvert(enabled=false)

# Double inversion must reproduce the source (bit-exact for integer formats).
ident8  = src.ZInvert().ZInvert()
ident10 = src10.ZInvert().ZInvert()
ident16 = src16.ZInvert().ZInvert()
identf  = srcf.ZInvert().ZInvert()
identy  = srcy.ZInvert().ZInvert()

# Planar RGBA: compare each plane (including alpha) as a grayscale clip.
identrgb = srcrgb.ZInvert().ZInvert()
rgb_r  = ExtractR(srcrgb)
rgb_g  = ExtractG(srcrgb)
rgb_b  = ExtractB(srcrgb)
rgb_a  = ExtractA(srcrgb)
irgb_r = ExtractR(identrgb)
irgb_g = ExtractG(identrgb)
irgb_b = ExtractB(identrgb)
irgb_a = ExtractA(identrgb)
inva   = ExtractA(srcrgb.ZInvert())

# ---- per-frame asserts, evaluated over every frame at script load ----
for (i = 0, frames) {
    global current_frame = i
    n = String(i)

    # Luma must match the built-in Invert() exactly; integer chroma may differ
    # by exactly 1 LSB (ZInvert pivots chroma at the midpoint like AviSynth+
    # 3.7.6, the built-in of the installed 3.7.5 still inverts full-scale).
    Assert(LumaDifference(inv, builtin) == 0.0, "luma differs from built-in Invert at frame " + n)
    Assert(ChromaUDifference(inv, builtin) <= 1.0, "U differs from built-in Invert by >1 LSB at frame " + n)
    Assert(ChromaVDifference(inv, builtin) <= 1.0, "V differs from built-in Invert by >1 LSB at frame " + n)

    # Inversion must actually change the picture.
    Assert(LumaDifference(inv, src) > 0.0, "ZInvert did not change the luma at frame " + n)

    # Average of (255 - x) must be 255 - average(x).
    Assert(Abs(AverageLuma(inv) - (255.0 - AverageLuma(src))) < 0.01, "inverted luma average is wrong at frame " + n)

    # enabled=false must pass the source through untouched.
    Assert(LumaDifference(pass, src) == 0.0, "pass-through altered the luma at frame " + n)
    Assert(ChromaUDifference(pass, src) == 0.0, "pass-through altered U at frame " + n)
    Assert(ChromaVDifference(pass, src) == 0.0, "pass-through altered V at frame " + n)

    # Double inversion is the identity (bit-exact for integer formats).
    Assert(LumaDifference(ident8, src) == 0.0, "8-bit double-invert luma not identity at frame " + n)
    Assert(ChromaUDifference(ident8, src) == 0.0, "8-bit double-invert U not identity at frame " + n)
    Assert(ChromaVDifference(ident8, src) == 0.0, "8-bit double-invert V not identity at frame " + n)
    Assert(LumaDifference(ident10, src10) == 0.0, "10-bit double-invert luma not identity at frame " + n)
    Assert(ChromaUDifference(ident10, src10) == 0.0, "10-bit double-invert U not identity at frame " + n)
    Assert(ChromaVDifference(ident10, src10) == 0.0, "10-bit double-invert V not identity at frame " + n)
    Assert(LumaDifference(ident16, src16) == 0.0, "16-bit double-invert luma not identity at frame " + n)
    Assert(ChromaUDifference(ident16, src16) == 0.0, "16-bit double-invert U not identity at frame " + n)
    Assert(ChromaVDifference(ident16, src16) == 0.0, "16-bit double-invert V not identity at frame " + n)
    Assert(LumaDifference(identy, srcy) == 0.0, "grayscale double-invert not identity at frame " + n)

    # Float: 1.0-(1.0-x) rounds, so allow a tiny epsilon; chroma negation is exact.
    Assert(LumaDifference(identf, srcf) < 0.000001, "float double-invert luma not identity at frame " + n)
    Assert(ChromaUDifference(identf, srcf) == 0.0, "float double-invert U not identity at frame " + n)
    Assert(ChromaVDifference(identf, srcf) == 0.0, "float double-invert V not identity at frame " + n)

    # Planar RGBA: all four planes identical after double inversion, and a
    # single inversion must turn the fully opaque alpha (255) into 0.
    Assert(LumaDifference(irgb_r, rgb_r) == 0.0, "RGBA double-invert R not identity at frame " + n)
    Assert(LumaDifference(irgb_g, rgb_g) == 0.0, "RGBA double-invert G not identity at frame " + n)
    Assert(LumaDifference(irgb_b, rgb_b) == 0.0, "RGBA double-invert B not identity at frame " + n)
    Assert(LumaDifference(irgb_a, rgb_a) == 0.0, "RGBA double-invert A not identity at frame " + n)
    Assert(AverageLuma(rgb_a) == 255.0, "source alpha is not fully opaque at frame " + n)
    Assert(AverageLuma(inva) == 0.0, "inverted alpha is not zero at frame " + n)
}

# ---- the visible result: a 2x2 grid of source and inverted variants ----
invf_8   = srcf.ZInvert().ConvertBits(8)
invrgb_8 = srcrgb.ZInvert().RemoveAlphaPlane().ConvertToYUV444(matrix="Rec601")
row1 = StackHorizontal(src.Subtitle("source (8-bit YV24)"), inv.Subtitle("ZInvert (8-bit)"))
row2 = StackHorizontal(invf_8.Subtitle("ZInvert (float) -> 8-bit"), invrgb_8.Subtitle("ZInvert (planar RGBA) -> 8-bit"))
grid = StackVertical(row1, row2)

grid.Subtitle("ZInvert: all asserts passed (" + String(frames + 1) + " frames)", align=2)
ConvertToRGB24(matrix="Rec601")
