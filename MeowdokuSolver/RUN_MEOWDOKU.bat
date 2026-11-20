```bat
@echo off
setlocal EnableExtensions
title MEOWDOKU AUTOPILOT

cd /d "%~dp0"

set "ROOT=%CD%"
set "SCREENSHOT_DIR=%ROOT%\assets\screenshots"
set "PYTHON=%ROOT%\.venv\bin\python.exe"
set "CONTROLLER=%ROOT%\tools\game_automation.py"
set "CAPTURE_SCRIPT=%ROOT%\tools\capture.py"
set "PLACE_SCRIPT=%ROOT%\tools\place_solution.py"
set "SCREEN_DETECTOR=%ROOT%\tools\screen_detector.py"
set "SOLVER=%ROOT%\build\Release\MeowdokuSolver.exe"
set "OPENCV_BIN=%ROOT%\vcpkg_installed\x64-windows\bin"

echo.
echo ==========================================
echo          MEOWDOKU AUTOPILOT
echo ==========================================
echo.

echo [1/7] Checking project directories...

if not exist "%SCREENSHOT_DIR%\" goto ERROR_SCREENSHOT_DIR
if not exist "%ROOT%\tools\" goto ERROR_TOOLS_DIR
if not exist "%ROOT%\build\Release\" goto ERROR_BUILD_DIR

echo [2/7] Checking Python environment...

if not exist "%PYTHON%" goto ERROR_PYTHON

"%PYTHON%" --version
if errorlevel 1 goto ERROR_PYTHON

echo [3/7] Checking automation scripts...

if not exist "%CONTROLLER%" goto ERROR_CONTROLLER
if not exist "%CAPTURE_SCRIPT%" goto ERROR_CAPTURE
if not exist "%PLACE_SCRIPT%" goto ERROR_PLACE
if not exist "%SCREEN_DETECTOR%" goto ERROR_DETECTOR

echo [4/7] Checking C++ solver...

if not exist "%SOLVER%" goto ERROR_SOLVER

echo [5/7] Checking ADB installation...

where adb >nul 2>&1
if errorlevel 1 goto ERROR_ADB

adb version

echo [6/7] Checking Android connection...

adb get-state >nul 2>&1
if errorlevel 1 goto ERROR_DEVICE

echo.
echo [ADB] Android device connected successfully!

adb shell wm size

echo.
echo [7/7] Preparing autopilot environment...

if not exist "%OPENCV_BIN%\" goto ERROR_OPENCV

set "PATH=%OPENCV_BIN%;%PATH%"

echo.
echo [CLEANUP] Removing only temporary generated files...

del /q "%SCREENSHOT_DIR%\phone_fullscreen.png" 2>nul
del /q "%SCREENSHOT_DIR%\current_puzzle.png" 2>nul
del /q "%SCREENSHOT_DIR%\region_detected.png" 2>nul
del /q "%SCREENSHOT_DIR%\solution_visualized.png" 2>nul
del /q "%SCREENSHOT_DIR%\grid_detected.png" 2>nul
del /q "%SCREENSHOT_DIR%\solution_taps.json" 2>nul

echo [CLEANUP] Reference screenshots preserved.

echo.
echo ==========================================
echo       STARTING GAME AUTOMATION
echo ==========================================
echo.
echo Maximum placement sequences: 50
echo Controller: game_automation.py
echo.
echo Press CTRL+C to stop automation.
echo.

"%PYTHON%" "%CONTROLLER%" --max-levels 50

if errorlevel 1 goto ERROR_AUTOMATION

echo.
echo ==========================================
echo       AUTOPILOT SESSION COMPLETE
echo ==========================================
echo.
echo Automation controller exited successfully.
echo.

pause
exit /b 0


:ERROR_SCREENSHOT_DIR
echo ERROR: Screenshot directory missing.
goto FAILED

:ERROR_TOOLS_DIR
echo ERROR: Tools directory missing.
goto FAILED

:ERROR_BUILD_DIR
echo ERROR: Build directory missing.
goto FAILED

:ERROR_PYTHON
echo ERROR: Project Python environment unavailable.
goto FAILED

:ERROR_CONTROLLER
echo ERROR: game_automation.py missing.
goto FAILED

:ERROR_CAPTURE
echo ERROR: capture.py missing.
goto FAILED

:ERROR_PLACE
echo ERROR: place_solution.py missing.
goto FAILED

:ERROR_DETECTOR
echo ERROR: screen_detector.py missing.
goto FAILED

:ERROR_SOLVER
echo ERROR: C++ solver executable missing.
goto FAILED

:ERROR_ADB
echo ERROR: ADB not found in PATH.
goto FAILED

:ERROR_DEVICE
echo ERROR: Android device disconnected or unauthorized.
goto FAILED

:ERROR_OPENCV
echo ERROR: OpenCV runtime directory missing.
goto FAILED

:ERROR_AUTOMATION
echo ERROR: Game automation controller failed.
goto FAILED

:FAILED
echo.
echo ==========================================
echo          AUTOPILOT FAILED
echo ==========================================
echo.
pause
exit /b 1
```