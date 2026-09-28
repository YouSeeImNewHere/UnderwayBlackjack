from PIL import Image, ImageDraw, ImageFont
import os

OUT_DIR = os.path.dirname(os.path.abspath(__file__))

FELT_GREEN = (26, 74, 34, 255)
WHITE = (245, 245, 245, 255)

FONT_CANDIDATES = [
    r"C:\Windows\Fonts\arialbd.ttf",
    r"C:\Windows\Fonts\arial.ttf",
    # Arial-metric-compatible fallbacks for Linux/macOS
    "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
    "/Library/Fonts/Arial Bold.ttf",
]

def make_icon(size, path, alpha=True):
    img = Image.new("RGBA", (size, size), FELT_GREEN)
    draw = ImageDraw.Draw(img)

    margin = size * 0.08
    draw.rounded_rectangle(
        [margin, margin, size - margin, size - margin],
        radius=size * 0.12,
        outline=WHITE,
        width=max(2, int(size * 0.02)),
    )

    font = None
    for candidate in FONT_CANDIDATES:
        if os.path.exists(candidate):
            font = ImageFont.truetype(candidate, int(size * 0.42))
            break
    if font is None:
        font = ImageFont.load_default()

    text = "21"
    bbox = draw.textbbox((0, 0), text, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    draw.text(
        ((size - tw) / 2 - bbox[0], (size - th) / 2 - bbox[1]),
        text,
        font=font,
        fill=WHITE,
    )

    if not alpha:
        # App Store icons must be fully opaque (no alpha channel at all)
        img = img.convert("RGB")
    img.save(path)
    print("wrote", path)

make_icon(192, os.path.join(OUT_DIR, "icon-192.png"))
make_icon(512, os.path.join(OUT_DIR, "icon-512.png"))

IOS_ICON_DIR = os.path.join(OUT_DIR, "..", "..", "ios", "Assets.xcassets", "AppIcon.appiconset")
make_icon(1024, os.path.join(IOS_ICON_DIR, "icon-1024.png"), alpha=False)
