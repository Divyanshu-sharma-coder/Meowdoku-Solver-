```bat
@echo off
setlocal EnableExtensions

title Meowdoku Solver - Auto Capture and Solve

REM ==========================================
REM MEOWDOKU AUTOMATION
REM ==========================================

cd /d "%~dp0"

set "ROOT=%CD%"
set "SCREENSHOT_DIR=%ROOT%\assets\screenshots"
set "CAPTURE_SCRIPT=%ROOT%\tools\capture.py"
set "PYTHON=%ROOT%\.venv\bin\python.exe"
set "SOLVER=%ROOT%\build\Release\MeowdokuSolver.exe"
set "OPENCV_BIN=%ROOT%\vcpkg_installed\x64-windows\bin"

echo.
echo ==========================================
echo       MEOWDOKU SOLVER AUTOMATION
echo ==========================================
echo.

REM ==========================================
REM STEP 1: CHECK REQUIRED FILES
REM ==========================================

echo [1/5] Checking project files...

if not exist "%SCREENSHOT_DIR%\" goto ERROR_SCREENSHOT_DIR
if not exist "%CAPTURE_SCRIPT%" goto ERROR_CAPTURE
if not exist "%PYTHON%" goto ERROR_PYTHON
if not exist "%SOLVER%" goto ERROR_SOLVER

where adb >nul 2>&1
if errorlevel 1 goto ERROR_ADB

REM ==========================================
REM STEP 2: CHECK ANDROID CONNECTION
REM ==========================================

echo.
echo [2/5] Checking Android connection...

adb get-state >nul 2>&1
if errorlevel 1 goto ERROR_DEVICE

echo [ADB] Android device connected!

REM ==========================================
REM STEP 3: CLEAN OLD GENERATED IMAGES
REM ==========================================

echo.
echo [3/5] Cleaning previous screenshots...

for %%E in (png jpg jpeg bmp webp) do (
    del /q "%SCREENSHOT_DIR%\*.%%E" 2>nul
)

echo [CLEAN] Old screenshot images removed!

REM ==========================================
REM STEP 4: CAPTURE PHONE SCREEN
REM ==========================================

echo.
echo [4/5] Capturing Android screen...

"%PYTHON%" "%CAPTURE_SCRIPT%" 1

if errorlevel 1 goto ERROR_CAPTURE_RUN

if not exist "%SCREENSHOT_DIR%\phone_fullscreen.png" goto ERROR_IMAGE

echo [CAPTURE] Screenshot captured successfully!

REM ==========================================
REM STEP 5: RUN C++ SOLVER
REM ==========================================

echo.
echo [5/5] Starting Meowdoku Solver...

if exist "%OPENCV_BIN%\" (
    set "PATH=%OPENCV_BIN%;%PATH%"
)

"%SOLVER%" "%SCREENSHOT_DIR%\phone_fullscreen.png"

if errorlevel 1 goto ERROR_SOLVER_RUN

REM ==========================================
REM SUCCESS
REM ==========================================

echo.
echo ==========================================
echo          PUZZLE PROCESSING COMPLETE
echo ==========================================
echo.

if exist "%SCREENSHOT_DIR%\solution_visualized.png" (
    echo Opening solution image...
    start "" "%SCREENSHOT_DIR%\solution_visualized.png"
) else (
    echo WARNING: Solution image was not generated.
)

echo.
echo Move to your next level and run this BAT again!
echo.
pause
exit /b 0


REM ==========================================
REM ERROR HANDLING
REM ==========================================

:ERROR_SCREENSHOT_DIR
echo ERROR: Screenshot directory missing.
goto FAILED

:ERROR_CAPTURE
echo ERROR: tools\capture.py missing.
goto FAILED

:ERROR_PYTHON
echo ERROR: Project Python executable missing.
goto FAILED

:ERROR_SOLVER
echo ERROR: MeowdokuSolver.exe missing.
echo Build using:
echo cmake --build build --config Release --parallel 2
goto FAILED

:ERROR_ADB
echo ERROR: ADB is not installed or not in PATH.
goto FAILED

:ERROR_DEVICE
echo ERROR: Android device not connected or authorized.
echo Run adb devices to check.
goto FAILED

:ERROR_CAPTURE_RUN
echo ERROR: Screenshot capture failed.
goto FAILED

:ERROR_IMAGE
echo ERROR: Screenshot image was not generated.
goto FAILED

:ERROR_SOLVER_RUN
echo ERROR: C++ solver execution failed.
goto FAILED

:FAILED
echo.
echo ==========================================
echo             PROCESS FAILED
echo ==========================================
echo Fix the error and try again.
echo.
pause
exit /b 1
```