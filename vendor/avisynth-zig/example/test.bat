@echo off
setlocal
cd /d "%~dp0"

set "RESULT=1"

where ffmpeg >nul 2>nul || (echo ERROR: ffmpeg not found in PATH. & goto :done)

if not exist "zig-out\bin\invert_example.dll" (
    echo ERROR: plugin not built. Run: zig build -Doptimize=ReleaseFast
    goto :done
)

echo Loading test.avs (runs every Assert over all frames) and decoding with ffmpeg...
ffmpeg -hide_banner -loglevel error -y -i test.avs -update 1 test_result.png
rem ffmpeg reports failures with NEGATIVE exit codes, which "if errorlevel 1"
rem does not catch - compare against 0 instead.
if %errorlevel% neq 0 (
    echo.
    echo TEST FAILED: an Assert fired or the script could not be loaded.
    goto :done
)

set "RESULT=0"
echo TEST PASSED: result image saved to test_result.png

:done
echo.
pause
exit /b %RESULT%
