import os
import sys
import subprocess
import struct
import re
import time

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

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

SCREENSHOTS = os.path.join(
    ROOT,
    "assets",
    "screenshots"
)

TESSERACT = r"C:\Program Files\Tesseract-OCR\tesseract.exe"

ADB = "adb"


# ============================================================
# KNOWN GAME BUTTON COORDINATES
# ============================================================

BUTTONS = {
    "LEADERBOARD": (542, 2138),
    "LEVEL_SELECT": (540, 1872),
    "GOLDEN_FISH": (538, 2231),
}


# ============================================================
# PNG SCREEN DIMENSIONS
# ============================================================

def get_screen_size(image_path):
    """Read PNG dimensions without external Python libraries."""

    with open(image_path, "rb") as file:

        header = file.read(24)

    if header[:8] != b"\x89PNG\r\n\x1a\n":

        raise ValueError(
            "Screenshot is not a valid PNG file."
        )

    width, height = struct.unpack(
        ">II",
        header[16:24]
    )

    return width, height


# ============================================================
# SAFE SUBPROCESS EXECUTION
# ============================================================

def run_process(command, timeout=30, binary=False):
    """
    Execute external programs safely.

    Binary mode is used for ADB screenshot capture.
    Text mode uses UTF-8 with replacement for invalid bytes.
    """

    if binary:

        return subprocess.run(
            command,
            capture_output=True,
            check=True,
            timeout=timeout
        )

    return subprocess.run(
        command,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=True,
        timeout=timeout
    )


# ============================================================
# ANDROID SCREEN CAPTURE
# ============================================================

def capture_screen():

    os.makedirs(
        SCREENSHOTS,
        exist_ok=True
    )

    output = os.path.join(
        SCREENSHOTS,
        "phone_fullscreen.png"
    )

    print("\n[SCREEN] Capturing Android screen...")

    result = run_process(
        [
            ADB,
            "exec-out",
            "screencap",
            "-p"
        ],
        timeout=20,
        binary=True
    )

    if not result.stdout:

        raise RuntimeError(
            "ADB returned an empty screenshot."
        )

    with open(output, "wb") as file:

        file.write(result.stdout)

    print("[SCREEN] Screenshot saved:", output)

    return output


# ============================================================
# TESSERACT OCR
# ============================================================

def run_ocr(image_path):

    if not os.path.exists(TESSERACT):

        raise FileNotFoundError(
            f"Tesseract executable not found: {TESSERACT}"
        )

    print("[OCR] Running Tesseract...")

    result = run_process(
        [
            TESSERACT,
            image_path,
            "stdout",
            "--psm",
            "11"
        ],
        timeout=45
    )

    # Never call .lower() directly on potentially missing output.
    output = result.stdout or ""

    if not isinstance(output, str):
        output = str(output)

    text = output.lower()

    # Normalize whitespace.
    text = re.sub(
        r"\s+",
        " ",
        text
    ).strip()

    print("[OCR] Recognition completed.")

    return text


# ============================================================
# SCREEN CLASSIFICATION
# ============================================================

def classify_screen(normalized, menu_context=False):
    normalized = (normalized or "").lower()

    if "leaderboard" in normalized:
        return "LEADERBOARD"

    if "golden fish" in normalized:
        return "GOLDEN_FISH"

    known_level_phrases = [
        "puzzle pro",
        "single slip-up",
        "not a single slip",
        "swift and flawless victory",
        "superhard level"
    ]

    if any(phrase in normalized for phrase in known_level_phrases):
        return "LEVEL_SELECT"

    # Only apply this fallback when we KNOW we are on a menu.
    if menu_context:
        return "LEVEL_SELECT"

    return "UNKNOWN"
# ============================================================
# MAIN SCREEN DETECTOR
# ============================================================

def detect_screen(image_path, menu_context=False):

    if not os.path.exists(image_path):

        raise FileNotFoundError(image_path)

    width, height = get_screen_size(image_path)

    text = run_ocr(image_path)

    normalized = " ".join(
        (text or "").split()
    )

    state = classify_screen(normalized, menu_context)

    button = BUTTONS.get(state)

    print("\n========== SCREEN DETECTION ==========")

    print(
        "Resolution:",
        width,
        "x",
        height
    )

    print(
        "OCR:",
        normalized[:500] if normalized else "[NO TEXT DETECTED]"
    )

    print(
        "Detected state:",
        state
    )

    if button:

        print(
            "Button coordinates:",
            button
        )

    print("=====================================\n")

    return {
        "state": state,
        "button": button,
        "width": width,
        "height": height,
        "text": normalized,
    }


# ============================================================
# STANDALONE EXECUTION
# ============================================================

if __name__ == "__main__":

    try:

        if len(sys.argv) > 1:

            image_path = sys.argv[1]

        else:

            image_path = capture_screen()

        detect_screen(image_path)

    except subprocess.CalledProcessError as error:

        print(
            "\n[SCREEN ERROR] External command failed:",
            error
        )

        sys.exit(1)

    except Exception as error:

        print(
            "\n[SCREEN ERROR]",
            error
        )

        sys.exit(1)
