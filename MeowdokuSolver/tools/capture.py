import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SCREENSHOT_DIR = ROOT / "assets" / "screenshots"


def capture_screen():
    print("\n[ADB] Capturing Android screen...")

    result = subprocess.run(
        ["adb", "exec-out", "screencap", "-p"],
        capture_output=True
    )

    if result.returncode != 0:
        raise RuntimeError("ADB screenshot capture failed.")

    if not result.stdout:
        raise RuntimeError("Empty screenshot received.")

    return result.stdout


def main():
    if len(sys.argv) < 2:
        print("Usage: python capture.py LEVEL")
        return

    level = sys.argv[1]

    SCREENSHOT_DIR.mkdir(parents=True, exist_ok=True)

    image_data = capture_screen()

    output = SCREENSHOT_DIR / "phone_fullscreen.png"

    output.write_bytes(image_data)

    print("\n========== CAPTURE COMPLETE ==========")
    print(f"Saved: {output}")
    print(f"Image size: {len(image_data)} bytes")
    print("=======================================")


if __name__ == "__main__":
    main()