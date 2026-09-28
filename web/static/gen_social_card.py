from PIL import Image, ImageDraw, ImageFont
import os

OUT_DIR = os.path.dirname(os.path.abspath(__file__))

FELT_GREEN = (26, 74, 34, 255)
WHITE = (245, 245, 245, 255)

FONT_CANDIDATES_BOLD = [
    r"C:\Windows\Fonts\arialbd.ttf",
    r"C:\Windows\Fonts\arial.ttf",
]
FONT_CANDIDATES_REGULAR = [
    r"C:\Windows\Fonts\arial.ttf",
]

def load_font(candidates, size):
    for c in candidates:
        if os.path.exists(c):
            return ImageFont.truetype(c, size)
    return ImageFont.load_default()

W, H = 1200, 630
img = Image.new("RGB", (W, H), FELT_GREEN)
draw = ImageDraw.Draw(img)

margin = 40
draw.rounded_rectangle(
    [margin, margin, W - margin, H - margin],
    radius=36,
    outline=WHITE,
    width=4,
)

title_font = load_font(FONT_CANDIDATES_BOLD, 64)
subtitle_font = load_font(FONT_CANDIDATES_REGULAR, 34)
big_font = load_font(FONT_CANDIDATES_BOLD, 200)

# Big "21" as the visual anchor, left side
text = "21"
bbox = draw.textbbox((0, 0), text, font=big_font)
tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
draw.text((140 - bbox[0], H / 2 - th / 2 - bbox[1]), text, font=big_font, fill=WHITE)

# Title + subtitle, right side
title = "Underway"
title2 = "Blackjack"
subtitle = "Play in your browser"

block_x = 460
draw.text((block_x, H / 2 - 130), title, font=title_font, fill=WHITE)
draw.text((block_x, H / 2 - 50), title2, font=title_font, fill=WHITE)
draw.text((block_x, H / 2 + 60), subtitle, font=subtitle_font, fill=(200, 220, 200, 255))

out_path = os.path.join(OUT_DIR, "social-card.png")
img.save(out_path)
print("wrote", out_path)
