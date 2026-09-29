# HDRAGC2 smoke test (1:1 port of HDRAGC 0.1.5)
# Run with AviSynth+ (64-bit) with the plugin loaded.

LoadCPlugin("..\zig-out\bin\hdragc2.dll")

# Slowly brightening synthetic frames (adaptive gain + temporal test)
blank = BlankClip(width=320, height=240, length=25, pixel_type="RGB32", fps=25, color=$202020)
gray  = BlankClip(width=320, height=240, length=25, pixel_type="RGB32", fps=25, color=$606060)

src = Merge(blank, gray)  # 12.5% abu -> 37.5% abu

# Default gain for a dark frame should be > 1 (dark -> lifted)
a = HDRAGC(src)
Assert(a, "HDRAGC default run")

b = HDRAGC(src, avg_lum=100, max_gain=2.5, circle=5, mode=0, avg_window=4, response=50)
Assert(b, "HDRAGC custom params run")

# Bright clip test (gain should shrink / equal 1)
bright = BlankClip(width=320, height=240, length=5, pixel_type="RGB32", fps=25, color=$D0D0D0)
c = HDRAGC(bright)
Assert(c, "HDRAGC bright clip run")

Return MessageClip("HDRAGC2 smoke test OK")
