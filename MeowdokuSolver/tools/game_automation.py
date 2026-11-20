import argparse
import json
import os
import shutil
import subprocess
import sys
import time
from pathlib import Path


# ============================================================
# WINDOWS UTF-8 CONFIGURATION
# ============================================================

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")


# ============================================================
# PROJECT CONFIGURATION
# ============================================================

ROOT = Path(__file__).resolve().parent.parent
SCREENSHOTS = ROOT / "assets" / "screenshots"

PYTHON = ROOT / ".venv" / "bin" / "python.exe"
SOLVER = ROOT / "build" / "Release" / "MeowdokuSolver.exe"

SCREEN_DETECTOR = ROOT / "tools" / "screen_detector.py"
PLACE_SCRIPT = ROOT / "tools" / "place_solution.py"

PHONE_SCREEN = SCREENSHOTS / "phone_fullscreen.png"
SOLUTION_FILE = SCREENSHOTS / "solution_taps.json"
DEBUG_DIR = SCREENSHOTS / "unknown"

OPENCV_BIN = ROOT / "vcpkg_installed" / "x64-windows" / "bin"


# Known game navigation buttons.
BUTTONS = {
    "LEADERBOARD": (542, 2138),
    "LEVEL_SELECT": (540, 1872),
    "GOLDEN_FISH": (538, 2231),
}

REFERENCE_WIDTH = 1080
REFERENCE_HEIGHT = 2412

# How many times in a row an unrecognised screen may be
# "advanced" through its orange button before giving up.
MAX_UNKNOWN_ADVANCES = 3

# How many times an unrecognised screen is re-captured
# (animations, transitions) before giving up.
MAX_UNKNOWN_RETRIES = 2


# ============================================================
# LOGGING
# ============================================================

def log(message):
    print(f"\n[AUTOPILOT] {message}", flush=True)


# ============================================================
# SUBPROCESS EXECUTION
# ============================================================

def run_command(command, timeout=120, env=None):

    return subprocess.run(
        command,
        cwd=ROOT,
        env=env,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout
    )


# ============================================================
# SCREEN STATE HELPERS
# ============================================================

def get_state(result):

    if isinstance(result, dict):
        state = result.get("state", "UNKNOWN")

        if isinstance(state, str):
            return state

    return "UNKNOWN"


def get_orange_button(result):

    if isinstance(result, dict):
        button = result.get("orange_button")

        if button:
            return button

    return None


# ============================================================
# SCREEN CAPTURE AND DETECTION
# ============================================================

def capture(menu_context=False):

    from screen_detector import capture_screen, detect_screen

    screenshot = capture_screen()

    result = detect_screen(
        screenshot,
        menu_context=menu_context
    )

    if not isinstance(result, dict):
        raise RuntimeError("Screen detector returned an invalid result.")

    return result


# ============================================================
# GENERIC "WAIT UNTIL THE SCREEN BECOMES X"
# ============================================================

def wait_until(condition, timeout=25, interval=2, stable=2):
    """
    Captures the screen repeatedly until condition(result) is
    True `stable` times in a row (this skips animation frames).
    Returns the last result, or None on timeout.
    """

    start = time.time()

    hits = 0

    while time.time() - start < timeout:

        time.sleep(interval)

        result = capture()

        log(f"Screen after tap: {get_state(result)}")

        if condition(result):

            hits += 1

            if hits >= stable:
                return result

        else:

            hits = 0

    return None


# ============================================================
# ANDROID DEVICE RESOLUTION
# ============================================================

_DEVICE_SIZE = None


def get_device_size():

    global _DEVICE_SIZE

    # The size never changes during a run, so ask ADB once.
    if _DEVICE_SIZE is not None:
        return _DEVICE_SIZE

    result = run_command(
        ["adb", "shell", "wm", "size"],
        timeout=15
    )

    if result.returncode != 0:
        raise RuntimeError("Could not read Android screen resolution.")

    import re

    matches = re.findall(
        r"(?:Physical|Override) size:\s*(\d+)x(\d+)",
        result.stdout
    )

    if not matches:
        raise RuntimeError("Could not determine Android resolution.")

    _DEVICE_SIZE = tuple(map(int, matches[-1]))

    return _DEVICE_SIZE


# ============================================================
# SCREEN TAPPING
# ============================================================

def tap_screen(x, y, screenshot_width, screenshot_height):

    if screenshot_width <= 0 or screenshot_height <= 0:
        raise RuntimeError("Invalid screenshot dimensions.")

    device_width, device_height = get_device_size()

    scaled_x = round(x * device_width / screenshot_width)
    scaled_y = round(y * device_height / screenshot_height)

    if not (
        0 <= scaled_x < device_width
        and 0 <= scaled_y < device_height
    ):
        raise RuntimeError("Invalid screen button coordinates.")

    log(f"Sending ADB tap: ({scaled_x}, {scaled_y})")

    result = run_command(
        [
            "adb",
            "shell",
            "input",
            "tap",
            str(scaled_x),
            str(scaled_y)
        ],
        timeout=15
    )

    if result.returncode != 0:
        raise RuntimeError(
            f"ADB tap failed: {result.stderr}"
        )

    log("ADB tap command completed successfully.")

    log("Waiting 5 seconds for the game animation...")

    time.sleep(5)

    log("Tap delay completed.")


# ============================================================
# MENU HANDLING
# ============================================================

def handle_menu(screen_result):

    state = get_state(screen_result)

    button = BUTTONS.get(state)

    if not button:
        log(f"No button configured for: {state}")
        return None

    x, y = button

    width = screen_result.get("width", 1080)
    height = screen_result.get("height", 2412)

    log(f"Detected: {state}")
    log(f"Original button coordinates: ({x}, {y})")

    tap_screen(x, y, width, height)

    # The screen that follows a tap may be ANYTHING: another
    # menu, a result screen whose text OCR cannot read, or the
    # puzzle itself.  So the only thing verified here is that
    # the screen is no longer the one that was tapped.
    # The main loop decides what the new screen is.

    log("Waiting for the screen to change...")

    result = wait_until(
        lambda r: get_state(r) != state,
        timeout=25
    )

    if result is None:
        log("Could not verify the expected screen transition.")
        return None

    log(
        f"Transition confirmed: "
        f"{state} -> {get_state(result)}"
    )

    return result


# ============================================================
# UNRECOGNISED SCREEN: TAP THE ORANGE BUTTON
# ============================================================

def advance_unknown_screen(screen_result):
    """
    Used only after the puzzle solver has already rejected the
    screen.  If the screen has a big orange button, tap it.
    """

    button = get_orange_button(screen_result)

    if button is None:
        return None

    x, y = button

    width = screen_result.get("width", 1080)
    height = screen_result.get("height", 2412)

    log(
        f"Unrecognised screen has an orange button at ({x}, {y}). "
        f"Tapping it."
    )

    tap_screen(x, y, width, height)

    def moved_on(result):

        if get_state(result) != "UNKNOWN":
            return True

        new_button = get_orange_button(result)

        # Button disappeared -> new screen.
        if new_button is None:
            return True

        # Button jumped somewhere else -> new screen.
        return abs(new_button[1] - y) > 120

    result = wait_until(moved_on, timeout=25)

    if result is None:
        log("The orange button did not lead to a new screen.")

    return result


# ============================================================
# DEBUG SCREENSHOT
# ============================================================

def save_debug_screenshot():

    try:

        DEBUG_DIR.mkdir(parents=True, exist_ok=True)

        destination = DEBUG_DIR / f"unknown_{int(time.time())}.png"

        shutil.copy(PHONE_SCREEN, destination)

        log(f"Saved the unrecognised screen to: {destination}")

    except OSError as error:

        log(f"Could not save the debug screenshot: {error}")


# ============================================================
# PUZZLE SOLVING
# ============================================================

def solve_current_puzzle():

    log("Attempting C++ puzzle detection and solving...")

    # Remove stale solutions before running the solver.
    if SOLUTION_FILE.exists():
        SOLUTION_FILE.unlink()

    environment = os.environ.copy()

    environment["PATH"] = (
        str(OPENCV_BIN)
        + os.pathsep
        + environment.get("PATH", "")
    )

    environment["PYTHONUTF8"] = "1"
    environment["PYTHONIOENCODING"] = "utf-8"

    result = run_command(
        [
            str(SOLVER),
            str(PHONE_SCREEN)
        ],
        timeout=120,
        env=environment
    )

    print(result.stdout)

    if result.stderr:
        print(result.stderr)

    if result.returncode != 0:

        log("C++ solver returned an error.")

        return False

    # A fresh JSON file must exist.
    if not SOLUTION_FILE.exists():

        log("No fresh solution JSON was generated.")

        return False

    # Validate the generated solution.
    try:

        with open(SOLUTION_FILE, "r", encoding="utf-8") as file:
            data = json.load(file)

        taps = data.get("taps")

        if not isinstance(taps, list):

            log("Invalid taps list in solution JSON.")

            return False

        if not taps:

            log("Solution contains zero placements.")

            return False

        for tap in taps:

            if not isinstance(tap, dict):
                return False

            required_keys = ("row", "column", "x", "y")

            if not all(key in tap for key in required_keys):
                return False

            if not all(
                isinstance(tap[key], int)
                and not isinstance(tap[key], bool)
                for key in required_keys
            ):
                return False

    except (OSError, ValueError, TypeError) as error:

        log(f"Solution JSON validation failed: {error}")

        return False

    log(f"Valid solution generated: {len(taps)} placements.")

    # --------------------------------------------------------
    # EXECUTE PLACEMENTS
    # --------------------------------------------------------

    log("Executing cat placements...")

    placement = run_command(
        [
            str(PYTHON),
            str(PLACE_SCRIPT),
            "--execute"
        ],
        timeout=180
    )

    print(placement.stdout)

    if placement.stderr:
        print(placement.stderr)

    if placement.returncode != 0:

        log("Placement execution failed.")

        return False

    log("Placement sequence executed successfully.")

    return True


# ============================================================
# WAIT FOR PUZZLE COMPLETION
# ============================================================

def wait_for_result(timeout=90):

    log("Waiting for puzzle completion...")

    start = time.time()

    recognized_states = (
        "LEADERBOARD",
        "LEVEL_SELECT",
        "GOLDEN_FISH"
    )

    while time.time() - start < timeout:

        time.sleep(3)

        result = capture()

        state = get_state(result)

        # A recognised menu state, OR an unrecognised screen that
        # carries the big orange button, both mean "level finished".
        if state in recognized_states or get_orange_button(result):

            log(f"Puzzle transition detected: {state}")

            return result

    log("No completion screen detected within timeout.")

    return None


# ============================================================
# ENVIRONMENT VALIDATION
# ============================================================

def validate_environment():

    required_paths = {
        "Project Python": PYTHON,
        "C++ Solver": SOLVER,
        "Screenshots Directory": SCREENSHOTS,
        "Screen Detector": SCREEN_DETECTOR,
        "Tap Executor": PLACE_SCRIPT,
    }

    for name, path in required_paths.items():

        if not path.exists():

            raise RuntimeError(
                f"{name} missing: {path}"
            )

    subprocess.run(
        ["adb", "get-state"],
        check=True,
        timeout=15
    )


# ============================================================
# MAIN AUTOPILOT CONTROLLER
# ============================================================

def main():

    parser = argparse.ArgumentParser(
        description="Meowdoku Puzzle Automation Controller"
    )

    parser.add_argument(
        "--max-levels",
        type=int,
        default=50
    )

    args = parser.parse_args()

    if args.max_levels < 1:

        raise RuntimeError(
            "Maximum levels must be positive."
        )

    validate_environment()

    sys.path.insert(0, str(ROOT / "tools"))

    log("======================================")
    log("       MEOWDOKU AUTOPILOT STARTED")
    log("======================================")

    completed_levels = 0

    # A pending result avoids unnecessary recaptures after
    # a verified navigation transition.
    pending_result = None

    unknown_retries = 0
    unknown_advances = 0

    while completed_levels < args.max_levels:

        log("Capturing current game screen...")

        if pending_result is not None:

            result = pending_result

            pending_result = None

        else:

            result = capture()

        state = get_state(result)

        log(f"Current screen state: {state}")

        # ----------------------------------------------------
        # KNOWN NAVIGATION SCREENS
        # ----------------------------------------------------

        if state in BUTTONS:

            unknown_retries = 0

            next_screen = handle_menu(result)

            if next_screen is None:

                log("Could not verify navigation.")

                log("Stopping safely.")

                break

            pending_result = next_screen

            continue

        # ----------------------------------------------------
        # POSSIBLE PLAYABLE PUZZLE
        # ----------------------------------------------------

        if state == "UNKNOWN":

            log("Unknown screen detected.")

            log("Testing whether this is a playable puzzle...")

            solved = solve_current_puzzle()

            if solved:

                unknown_retries = 0
                unknown_advances = 0

                completed_levels += 1

                log(
                    f"Placement sequence completed. "
                    f"Progress: {completed_levels}/{args.max_levels}"
                )

                next_screen = wait_for_result()

                if next_screen is None:

                    log("No recognized completion screen appeared.")

                    log("Stopping safely.")

                    break

                pending_result = next_screen

                continue

            # ------------------------------------------------
            # NOT A PUZZLE.  Probably a result/menu screen whose
            # text OCR could not read (the praise line changes).
            # ------------------------------------------------

            if (
                get_orange_button(result)
                and unknown_advances < MAX_UNKNOWN_ADVANCES
            ):

                unknown_advances += 1

                next_screen = advance_unknown_screen(result)

                if next_screen is None:

                    save_debug_screenshot()

                    log("Stopping safely.")

                    break

                pending_result = next_screen

                continue

            # ------------------------------------------------
            # Maybe just a transition frame: look again.
            # ------------------------------------------------

            if unknown_retries < MAX_UNKNOWN_RETRIES:

                unknown_retries += 1

                log(
                    f"Re-checking the screen "
                    f"({unknown_retries}/{MAX_UNKNOWN_RETRIES})..."
                )

                time.sleep(3)

                continue

            log("Could not confirm a valid playable puzzle.")

            save_debug_screenshot()

            log("Stopping safely instead of tapping blindly.")

            break

        # ----------------------------------------------------
        # UNRECOGNIZED STATE
        # ----------------------------------------------------

        log(f"Unexpected screen state: {state}")

        log("Stopping safely.")

        break

    # --------------------------------------------------------
    # SESSION SUMMARY
    # --------------------------------------------------------

    log("======================================")
    log("         AUTOPILOT SESSION ENDED")
    log("======================================")

    log(
        f"Completed placement sequences: "
        f"{completed_levels}/{args.max_levels}"
    )


# ============================================================
# ENTRY POINT
# ============================================================

if __name__ == "__main__":

    try:

        main()

    except KeyboardInterrupt:

        print("\n[STOP] Autopilot interrupted by user.")

    except Exception as error:

        print(f"\n[FATAL ERROR] {error}")

        sys.exit(1)
