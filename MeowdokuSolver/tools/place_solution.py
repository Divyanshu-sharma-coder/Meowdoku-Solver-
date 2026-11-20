import argparse
import json
import re
import struct
import subprocess
import sys
import time
from pathlib import Path

# Force UTF-8 output on Windows CMD
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

ROOT = Path(__file__).resolve().parent.parent

SCREENSHOT = ROOT / "assets" / "screenshots" / "phone_fullscreen.png"
SOLUTION = ROOT / "assets" / "screenshots" / "solution_taps.json"


def run_adb(*args):
    return subprocess.run(
        ["adb", *args],
        capture_output=True,
        text=True,
        check=True
    ).stdout.strip()


def get_screenshot_size():
    with open(SCREENSHOT, "rb") as file:
        header = file.read(24)

    if header[:8] != b"\x89PNG\r\n\x1a\n":
        raise RuntimeError("Invalid PNG screenshot.")

    width, height = struct.unpack(">II", header[16:24])

    return width, height


def get_device_size():
    output = run_adb("shell", "wm", "size")

    matches = re.findall(
        r"(?:Physical|Override) size:\s*(\d+)x(\d+)",
        output
    )

    if not matches:
        raise RuntimeError(
            f"Could not determine Android display size: {output}"
        )

    # Prefer the last reported size, which is the override if present.
    return tuple(map(int, matches[-1]))


def load_solution():
    if not SOLUTION.exists():
        raise RuntimeError("solution_taps.json was not generated.")

    with open(SOLUTION, "r", encoding="utf-8") as file:
        data = json.load(file)

    taps = data.get("taps")

    if not isinstance(taps, list):
        raise RuntimeError("Invalid solution JSON.")

    return taps


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "--execute",
        action="store_true",
        help="Actually tap the Android screen."
    )

    parser.add_argument(
        "--interval",
        type=float,
        default=0.15,
        help="Delay between taps in seconds."
    )

    args = parser.parse_args()

    if args.interval < 0.05 or args.interval > 2:
        raise RuntimeError("Interval must be between 0.05 and 2 seconds.")

    print("\n===================================")
    print("       MEOWDOKU TAP EXECUTOR")
    print("===================================")

    if not SCREENSHOT.exists():
        raise RuntimeError("Phone screenshot is missing.")

    run_adb("get-state")

    screenshot_width, screenshot_height = get_screenshot_size()
    device_width, device_height = get_device_size()

    taps = load_solution()

    scale_x = device_width / screenshot_width
    scale_y = device_height / screenshot_height

    print(f"\nScreenshot: {screenshot_width} x {screenshot_height}")
    print(f"Android:    {device_width} x {device_height}")
    print(f"Required taps: {len(taps)}")

    prepared = []

    for tap in taps:
        x = round(tap["x"] * scale_x)
        y = round(tap["y"] * scale_y)

        if not (0 <= x < device_width and 0 <= y < device_height):
            raise RuntimeError(f"Invalid tap coordinate: {x}, {y}")

        prepared.append((tap["row"], tap["column"], x, y))

    print("\nPlanned placements:")

    for row, column, x, y in prepared:
        print(
            f"Row {row}, Column {column} "
            f"-> Android ({x}, {y})"
        )

    if not args.execute:
        print("\nDRY RUN COMPLETE.")
        print("No taps were sent to the phone.")
        print("\nTo execute:")
        print("python tools/place_solution.py --execute")
        return

    print("\nStarting automatic placement...")

    for index, (row, column, x, y) in enumerate(prepared, 1):

        # First tap
        run_adb(
            "shell",
            "input",
            "tap",
            str(x),
            str(y)
        )

        time.sleep(0.08)

        # Second tap: converts the cross into a cat
        run_adb(
            "shell",
            "input",
            "tap",
            str(x),
            str(y)
        )

        print(
            f"[{index}/{len(prepared)}] "
            f"🐱 Placed cat at Row {row}, Column {column}"
        )

        time.sleep(args.interval)

    print("\n===================================")
    print("       PLACEMENT COMPLETE")
    print("===================================")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(f"\nERROR: {error}")
        sys.exit(1)