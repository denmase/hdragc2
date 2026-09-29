@echo off
REM Smoke test: render test\test_hdragc.avs to completion.
REM Replace avs2yuv with avspmod / avsmeter / mpv if not available.
zig build -Doptimize=ReleaseFast || exit /b 1
avs2yuv test\test_hdragc.avs -o NUL || exit /b 1
echo PASS
