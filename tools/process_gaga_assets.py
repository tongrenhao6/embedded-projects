"""Convert generated Gaga RGB PNGs into aligned RGBA layer candidates.

The source generator exported a visible checkerboard/white backdrop instead of
an alpha channel. This deterministic pass removes only near-neutral pixels that
are connected to the image border, preserving white parts enclosed by outlines.
"""

from __future__ import annotations

from pathlib import Path

import cv2
import numpy as np
from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets"
OUTPUT = SOURCE / "processed"
NAMES = (
    "gaga_no_arms.png",
    "gaga_arms_layer.png",
    "gaga_arms_raised.png",
    "gaga_arms_cheer.png",
    "gaga_eyes_happy.png",
    "gaga_eyes_normal.png",
    "gaga_eyes_sleepy.png",
    "gaga_mouth_closed.png",
    "gaga_mouth_open.png",
)


def border_connected_background(rgb: np.ndarray) -> np.ndarray:
    """Return pixels that look like the light neutral border background."""

    channels = rgb.astype(np.int16)
    brightness = channels.mean(axis=2)
    spread = channels.max(axis=2) - channels.min(axis=2)
    # Generated backgrounds are white/checkerboard neutrals. The dark outlines
    # around character details keep enclosed white areas out of this mask.
    # The generator's checkerboard contains both near-white and light-gray
    # squares. Neutral subject details remain protected by border connectivity.
    candidate = ((brightness >= 180) & (spread <= 28)).astype(np.uint8)

    # Keep only candidate pixels connected to any border. This avoids removing
    # the white belly, sclera, or highlights inside the character.
    count, labels, stats, _ = cv2.connectedComponentsWithStats(candidate, 8)
    connected = np.zeros_like(candidate, dtype=bool)
    border_labels = set(labels[0, :]) | set(labels[-1, :]) | set(labels[:, 0]) | set(labels[:, -1])
    for label in border_labels:
        if label != 0:
            connected[labels == label] = True
    return connected


def process(name: str) -> None:
    source = SOURCE / name
    image = Image.open(source).convert("RGB")
    rgb = np.asarray(image, dtype=np.uint8)
    background = border_connected_background(rgb)

    rgba = np.empty((rgb.shape[0], rgb.shape[1], 4), dtype=np.uint8)
    rgba[:, :, :3] = rgb
    rgba[:, :, 3] = np.where(background, 0, 255).astype(np.uint8)

    output = OUTPUT / name
    Image.fromarray(rgba, mode="RGBA").save(output, format="PNG", optimize=True)
    kept = int(np.count_nonzero(~background))
    print(f"{name}: size={image.size} opaque={kept} background={background.size - kept}")


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    for name in NAMES:
        process(name)


if __name__ == "__main__":
    main()
