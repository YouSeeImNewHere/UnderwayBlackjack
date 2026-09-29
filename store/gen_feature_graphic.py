# Google Play feature graphic (1024x500, no transparency) in the same style
# as web/static/social-card.png. Run: python3 store/gen_feature_graphic.py
from PIL import Image, ImageDraw, ImageFont
import os

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "feature-graphic.png")
W, H = 1024, 500
GREEN, WHITE, GOLD = (26, 74, 34), (245, 245, 245), (255, 225, 80)

def font(size, bold):
    for path in ([r"C:\Windows\Fonts\arialbd.ttf", "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf"] if bold
                 else [r"C:\Windows\Fonts\arial.ttf", "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"]):
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()

img = Image.new("RGB", (W, H), GREEN)
d = ImageDraw.Draw(img)
d.rounded_rectangle([28, 28, W - 28, H - 28], radius=36, outline=WHITE, width=4)
d.text((80, 130), "21", font=font(230, True), fill=WHITE)
d.text((410, 120), "Blackjack", font=font(92, True), fill=WHITE)
d.text((410, 220), "Variants", font=font(92, True), fill=GOLD)
d.text((414, 340), "5 games - strategy charts - card counting", font=font(25, False), fill=(200, 220, 200))
img.save(OUT)
print("wrote", OUT)
