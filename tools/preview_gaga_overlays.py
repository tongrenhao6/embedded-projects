from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "processed"
OUTPUT = SOURCE / "gaga_overlay_contact_sheet.png"
CROP = (399, 143, 1657, 1931)


def composite(*layers: str) -> Image.Image:
    canvas = Image.new("RGBA", (2048, 2048), (46, 52, 62, 255))
    for name in layers:
        canvas.alpha_composite(Image.open(SOURCE / name).convert("RGBA"))
    return canvas.convert("RGB").resize((512, 512), Image.Resampling.LANCZOS)


def firmware_preview(*layers: str) -> Image.Image:
    canvas = Image.new("RGBA", (2048, 2048), (46, 52, 62, 255))
    for name in layers:
        canvas.alpha_composite(Image.open(SOURCE / name).convert("RGBA"))
    return canvas.crop(CROP).resize((240, 280), Image.Resampling.LANCZOS).convert("RGB")


def main() -> None:
    combinations = (
        ("gaga_no_arms.png", "gaga_arms_layer.png"),
        ("gaga_no_arms.png", "gaga_arms_layer.png", "gaga_eyes_happy.png"),
        ("gaga_no_arms.png", "gaga_arms_layer.png", "gaga_eyes_sleepy.png"),
        ("gaga_no_arms.png", "gaga_arms_layer.png", "gaga_mouth_closed.png"),
        ("gaga_no_arms.png", "gaga_arms_layer.png", "gaga_mouth_open.png"),
    )
    labels = ("base + arms", "happy eyes", "sleepy eyes", "closed mouth", "open mouth")
    sheet = Image.new("RGB", (512 * len(combinations), 550), (28, 32, 40))
    draw = ImageDraw.Draw(sheet)
    for index, (layers, label) in enumerate(zip(combinations, labels)):
        sheet.paste(composite(*layers), (index * 512, 0))
        draw.text((index * 512 + 12, 526), label, fill=(245, 248, 250))
    sheet.save(OUTPUT, optimize=True)
    firmware_sheet = Image.new("RGB", (240 * 3, 320), (28, 32, 40))
    draw = ImageDraw.Draw(firmware_sheet)
    for index, name in enumerate(("gaga_arms_layer.png", "gaga_arms_raised.png", "gaga_arms_cheer.png")):
        firmware_sheet.paste(firmware_preview("gaga_no_arms.png", name), (index * 240, 0))
        draw.text((index * 240 + 6, 294), name.replace("gaga_", "").replace(".png", ""), fill=(245, 248, 250))
    firmware_sheet.save(SOURCE / "gaga_arms_firmware_preview.png", optimize=True)
    print(OUTPUT)


if __name__ == "__main__":
    main()
