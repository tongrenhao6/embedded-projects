"""Compose expression previews from the processed Gaga feature layers.

The AI-generated feature layers are not drawn at the base's scale, so every
layer is measured (eye span / mouth width), rescaled to the base face, and
re-centered. The base eye/mouth regions are inpainted to skin first so the
original features never bleed through.

Coordinates are on the 2048x2048 source canvas. Output is a firmware-like
preview: the character sprite (alpha-bbox crop, 280 px tall, aspect kept)
centered on a 480x320 dark canvas, plus a 2x face close-up row.
"""

from __future__ import annotations

from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "processed"
OUTPUT = SOURCE / "gaga_expression_preview.png"
DEBUG = ROOT / "tmp" / "expression_debug.png"

BASE = "gaga_no_arms.png"
EYES = {"normal": "gaga_eyes_normal.png", "happy": "gaga_eyes_happy.png", "sleepy": "gaga_eyes_sleepy.png"}
MOUTH = {"closed": "gaga_mouth_closed.png", "open": "gaga_mouth_open.png"}

# (label, eyes, mouth, eye vertical offset in source px)
STATES = [
    ("待机", "normal", "closed", 0),
    ("说话", "normal", "open", 0),
    ("开心", "happy", "open", -30),
    ("犯困", "sleepy", "closed", 0),
]

BG = (46, 52, 62)
SCREEN = (480, 320)
SPRITE_H = 280
FACE_ZOOM = (0.24, 0.05, 0.76, 0.48)  # fractions of the sprite box (l, t, r, b)
ZOOM = 2


def load_rgba(name: str) -> np.ndarray:
    return np.asarray(Image.open(SOURCE / name).convert("RGBA"), dtype=np.uint8)


def alpha_components(rgba: np.ndarray, min_area: int = 400) -> list[dict]:
    alpha = (rgba[:, :, 3] > 128).astype(np.uint8)
    count, _labels, stats, centroids = cv2.connectedComponentsWithStats(alpha, 8)
    comps = []
    for i in range(1, count):
        if stats[i, cv2.CC_STAT_AREA] >= min_area:
            comps.append(
                {
                    "bbox": [int(stats[i, cv2.CC_STAT_LEFT]), int(stats[i, cv2.CC_STAT_TOP]),
                             int(stats[i, cv2.CC_STAT_WIDTH]), int(stats[i, cv2.CC_STAT_HEIGHT])],
                    "center": (float(centroids[i][0]), float(centroids[i][1])),
                }
            )
    comps.sort(key=lambda c: c["center"][0])
    return comps


def detect_base_eyes(base_rgba: np.ndarray) -> list[dict]:
    hsv = cv2.cvtColor(base_rgba[:, :, :3], cv2.COLOR_RGB2HSV)
    iris = ((hsv[:, :, 0] >= 95) & (hsv[:, :, 0] <= 125) & (hsv[:, :, 1] > 60) & (hsv[:, :, 2] > 60)).astype(np.uint8)
    count, _labels, stats, centroids = cv2.connectedComponentsWithStats(iris, 8)
    idx = [i for i in range(1, count) if stats[i, cv2.CC_STAT_AREA] > 1500]
    idx.sort(key=lambda i: stats[i, cv2.CC_STAT_AREA], reverse=True)
    if len(idx) < 2:
        raise RuntimeError(f"expected 2 irises, found {len(idx)}")
    idx = sorted(idx[:2], key=lambda i: centroids[i][0])
    eyes = []
    for i in idx:
        x, y = int(stats[i, cv2.CC_STAT_LEFT]), int(stats[i, cv2.CC_STAT_TOP])
        w, h = int(stats[i, cv2.CC_STAT_WIDTH]), int(stats[i, cv2.CC_STAT_HEIGHT])
        # iris box -> full eye incl. sclera, outline and the upward lash sweep
        pad_x, pad_top, pad_bot = int(w * 0.55), int(h * 0.50), int(h * 0.30)
        eyes.append(
            {
                "bbox": [x - pad_x, y - pad_top, w + 2 * pad_x, h + pad_top + pad_bot],
                "center": (float(centroids[i][0]), float(centroids[i][1])),
            }
        )
    return eyes


def detect_base_mouth(base_rgba: np.ndarray, eyes: list[dict]) -> dict:
    """The mouth is the only small dark blob in the central face band below the eyes."""
    value = base_rgba[:, :, :3].max(axis=2)
    dark = (value < 100).astype(np.uint8)
    mid_x = (eyes[0]["center"][0] + eyes[1]["center"][0]) / 2
    mid_y = (eyes[0]["center"][1] + eyes[1]["center"][1]) / 2
    y0, y1 = int(mid_y + 60), int(mid_y + 200)
    x0, x1 = int(mid_x - 200), int(mid_x + 200)
    search = np.zeros_like(dark)
    search[y0:y1, x0:x1] = dark[y0:y1, x0:x1]
    count, _labels, stats, centroids = cv2.connectedComponentsWithStats(search, 8)
    idx = [i for i in range(1, count) if 200 <= stats[i, cv2.CC_STAT_AREA] <= 9000]
    if not idx:
        raise RuntimeError(f"mouth not found in band y={y0}..{y1} x={x0}..{x1}")
    l = min(int(stats[i, cv2.CC_STAT_LEFT]) for i in idx)
    t = min(int(stats[i, cv2.CC_STAT_TOP]) for i in idx)
    r = max(int(stats[i, cv2.CC_STAT_LEFT] + stats[i, cv2.CC_STAT_WIDTH]) for i in idx)
    b = max(int(stats[i, cv2.CC_STAT_TOP] + stats[i, cv2.CC_STAT_HEIGHT]) for i in idx)
    big = max(idx, key=lambda k: stats[k, cv2.CC_STAT_AREA])
    return {
        "bbox": [l, t, r - l, b - t],
        "center": (float(centroids[big][0]), float(centroids[big][1])),
    }


def paste_scaled(canvas_img: Image.Image, layer_rgba: np.ndarray, comps: list[dict],
                 scale: float, src_anchor: tuple[float, float], dst_anchor: tuple[float, float]) -> None:
    """Scale the layer around its feature bbox and pin src_anchor onto dst_anchor."""
    l, t, w, h = layer_bbox(comps)
    region = Image.fromarray(layer_rgba[t : t + h, l : l + w], "RGBA")
    new_size = (max(1, round(w * scale)), max(1, round(h * scale)))
    region = region.resize(new_size, Image.Resampling.LANCZOS)
    ax = (src_anchor[0] - l) * scale
    ay = (src_anchor[1] - t) * scale
    px, py = int(round(dst_anchor[0] - ax)), int(round(dst_anchor[1] - ay))
    canvas_img.alpha_composite(region, (px, py))


def layer_bbox(comps: list[dict]) -> tuple[int, int, int, int]:
    l = min(c["bbox"][0] for c in comps)
    t = min(c["bbox"][1] for c in comps)
    r = max(c["bbox"][0] + c["bbox"][2] for c in comps)
    b = max(c["bbox"][1] + c["bbox"][3] for c in comps)
    return (l, t, r - l, b - t)


def strip_skin(rgba: np.ndarray) -> np.ndarray:
    """Make warm light (skin-toned) pixels transparent so the layer carries
    only the feature: iris/sclera/lash survive, the surrounding skin patch goes."""
    out = rgba.copy()
    rgb = out[:, :, :3].astype(np.int16)
    r, g, b = rgb[:, :, 0], rgb[:, :, 1], rgb[:, :, 2]
    skin = ((r - b) >= 15) & (rgb.max(axis=2) >= 170)
    out[:, :, 3] = np.where(skin, 0, out[:, :, 3]).astype(np.uint8)
    return out


def inpaint_regions(rgb: np.ndarray, boxes: list[list[int]], pad: int = 10) -> np.ndarray:
    mask = np.zeros(rgb.shape[:2], dtype=np.uint8)
    for l, t, w, h in boxes:
        mask[max(0, t - pad) : t + h + pad, max(0, l - pad) : l + w + pad] = 255
    return cv2.inpaint(rgb, mask, 8, cv2.INPAINT_TELEA)


def font(size: int) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    for name in ("msyh.ttc", "simhei.ttf"):
        try:
            return ImageFont.truetype(f"C:/Windows/Fonts/{name}", size)
        except OSError:
            continue
    return ImageFont.load_default()


def render_panel(frame_rgba: np.ndarray, char_bbox: list[int]) -> Image.Image:
    l, t, w, h = char_bbox
    crop = Image.fromarray(frame_rgba[t : t + h, l : l + w], "RGBA")
    scale = SPRITE_H / h
    sprite = crop.resize((max(1, round(w * scale)), SPRITE_H), Image.Resampling.LANCZOS)
    panel = Image.new("RGBA", SCREEN, BG + (255,))
    panel.alpha_composite(sprite, ((SCREEN[0] - sprite.width) // 2, (SCREEN[1] - SPRITE_H) // 2))
    return panel


def main() -> None:
    base = load_rgba(BASE)
    eyes = detect_base_eyes(base)
    mouth = detect_base_mouth(base, eyes)

    eye_span = eyes[1]["center"][0] - eyes[0]["center"][0]
    eye_mid = ((eyes[0]["center"][0] + eyes[1]["center"][0]) / 2, (eyes[0]["center"][1] + eyes[1]["center"][1]) / 2)

    layers = {n: load_rgba(p) for n, p in {**EYES, **MOUTH}.items()}
    comps = {n: alpha_components(rgba) for n, rgba in layers.items()}

    def pair_anchor(name: str) -> tuple[float, float]:
        cs = comps[name]
        if len(cs) >= 2:
            return ((cs[0]["center"][0] + cs[-1]["center"][0]) / 2, (cs[0]["center"][1] + cs[-1]["center"][1]) / 2)
        c = layer_bbox(cs)
        return (c[0] + c[2] / 2, c[1] + c[3] / 2)

    def pair_span(name: str) -> float:
        cs = comps[name]
        if len(cs) >= 2:
            return cs[-1]["center"][0] - cs[0]["center"][0]
        return layer_bbox(cs)[2]

    mouth_w = mouth["bbox"][2]
    print(f"base: eye_span={eye_span:.0f}px eye_mid={eye_mid} mouth_w={mouth_w}px mouth_center={mouth['center']}")
    for name in layers:
        s, a = pair_span(name), pair_anchor(name)
        target = eye_span if name in EYES else mouth_w
        print(f"  {name:8s} span={s:7.0f}px anchor=({a[0]:.0f},{a[1]:.0f}) scale_needed={target / s:.3f}")

    erase_mouth = [mouth["bbox"]]
    erase_full = [e["bbox"] for e in eyes] + erase_mouth
    clean_mouth_rgb = inpaint_regions(base[:, :, :3].copy(), erase_mouth)
    clean_full_rgb = inpaint_regions(base[:, :, :3].copy(), erase_full)
    clean_mouth = np.dstack([clean_mouth_rgb, base[:, :, 3]]).astype(np.uint8)
    clean_full = np.dstack([clean_full_rgb, base[:, :, 3]]).astype(np.uint8)

    char_bbox = list(cv2.boundingRect((base[:, :, 3] > 128).astype(np.uint8)))

    debug = base[:, :, :3].copy()
    for e in eyes:
        l, t, w, h = e["bbox"]
        cv2.rectangle(debug, (l, t), (l + w, t + h), (0, 255, 0), 6)
    l, t, w, h = mouth["bbox"]
    cv2.rectangle(debug, (l, t), (l + w, t + h), (255, 80, 80), 6)
    Image.fromarray(debug).save(DEBUG)

    panels = []
    for label, eye_name, mouth_name, eye_dy in STATES:
        keep_eyes = eye_name == "normal"  # base eyes already are the normal look
        frame = Image.fromarray(clean_full if not keep_eyes else clean_mouth, "RGBA")
        if not keep_eyes:
            eye_scale = eye_span / pair_span(eye_name)
            paste_scaled(frame, strip_skin(layers[eye_name]), comps[eye_name], eye_scale,
                         pair_anchor(eye_name), (eye_mid[0], eye_mid[1] + eye_dy))
        mouth_scale = mouth_w / pair_span(mouth_name)
        paste_scaled(frame, layers[mouth_name], comps[mouth_name], mouth_scale, pair_anchor(mouth_name), mouth["center"])
        panel = render_panel(np.asarray(frame, dtype=np.uint8), char_bbox)
        panels.append((label, panel))

    # sheet: device row + 2x face close-up row
    label_h = 34
    zoom_w = int((FACE_ZOOM[2] - FACE_ZOOM[0]) * panels[0][1].width * ZOOM)
    zoom_h = int((FACE_ZOOM[3] - FACE_ZOOM[1]) * panels[0][1].height * ZOOM)
    sheet = Image.new("RGB", (SCREEN[0] * len(panels), SCREEN[1] + label_h + zoom_h + label_h), (24, 27, 33))
    draw = ImageDraw.Draw(sheet)
    try:
        ft = font(22)
    except Exception:
        ft = ImageFont.load_default()
    for i, (label, panel) in enumerate(panels):
        sheet.paste(panel.convert("RGB"), (i * SCREEN[0], 0))
        draw.text((i * SCREEN[0] + 12, SCREEN[1] + 6), label, fill=(235, 238, 242), font=ft)
        fx, fy = int(FACE_ZOOM[0] * panel.width), int(FACE_ZOOM[1] * panel.height)
        fw, fh = int((FACE_ZOOM[2] - FACE_ZOOM[0]) * panel.width), int((FACE_ZOOM[3] - FACE_ZOOM[1]) * panel.height)
        zoom = panel.crop((fx, fy, fx + fw, fy + fh)).resize((zoom_w, zoom_h), Image.Resampling.NEAREST)
        sheet.paste(zoom.convert("RGB"), (i * SCREEN[0], SCREEN[1] + label_h))
    sheet.save(OUTPUT, optimize=True)
    print(f"saved {OUTPUT} size={sheet.size}")


if __name__ == "__main__":
    main()
