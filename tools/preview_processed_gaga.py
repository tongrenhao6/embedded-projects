from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets" / "processed"
OUTPUT = SOURCE / "gaga_layer_contact_sheet.png"


def composite(name: str) -> Image.Image:
    base = Image.open(SOURCE / "gaga_no_arms.png").convert("RGBA")
    arms = Image.open(SOURCE / name).convert("RGBA")
    canvas = Image.new("RGBA", base.size, (46, 52, 62, 255))
    canvas.alpha_composite(base)
    canvas.alpha_composite(arms)
    return canvas.convert("RGB")


def main() -> None:
    names = ("gaga_arms_layer.png", "gaga_arms_raised.png", "gaga_arms_cheer.png")
    panels = [composite(name).resize((512, 512), Image.Resampling.LANCZOS) for name in names]
    sheet = Image.new("RGB", (512 * len(panels), 560), (28, 32, 40))
    draw = ImageDraw.Draw(sheet)
    for index, (name, panel) in enumerate(zip(names, panels)):
        left = index * 512
        sheet.paste(panel, (left, 0))
        draw.text((left + 12, 530), name, fill=(245, 248, 250))
    sheet.save(OUTPUT, optimize=True)
    print(OUTPUT)


if __name__ == "__main__":
    main()
