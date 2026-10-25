"""Draws the port's own icon (no game art: an orbit around a gold "2" on a night-blue field) as
.ico (every Windows size) and .png. The executables embed it (gr2-probe.rc); packaged builds use
the game's cover instead when the player makes one (make_game_icon.py). Needs Pillow.
usage: make_port_icon.py launcher/gr2_port.ico launcher/gr2_port.png"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFilter, ImageFont

S = 1024  # drawn large, scaled down for every icon size
FONT = Path(__file__).resolve().parents[2] / 'gpu' / 'third_party' / 'fonts' / 'DejaVuSans.ttf'


def draw():
    image = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    mask = Image.new('L', (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle((24, 24, S - 24, S - 24), radius=190, fill=255)

    field = Image.new('RGBA', (S, S))
    fd = ImageDraw.Draw(field)
    for y in range(S):  # night blue to violet
        t = y / (S - 1)
        fd.line([(0, y), (S, y)], fill=(int(18 + 60 * t), int(20 + 10 * t), int(58 + 60 * t), 255))
    image.paste(field, (0, 0), mask)

    glow = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse((150, 330, S - 150, S - 330), outline=(255, 196, 64, 255), width=46)
    glow = glow.rotate(-28, resample=Image.BICUBIC, center=(S / 2, S / 2))
    image.alpha_composite(glow.filter(ImageFilter.GaussianBlur(18)))
    image.alpha_composite(glow)

    text = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    td = ImageDraw.Draw(text)
    font = ImageFont.truetype(str(FONT), 620)
    box = td.textbbox((0, 0), '2', font=font)
    x = (S - (box[2] - box[0])) / 2 - box[0]
    y = (S - (box[3] - box[1])) / 2 - box[1]
    td.text((x + 14, y + 18), '2', font=font, fill=(0, 0, 0, 150))
    td.text((x, y), '2', font=font, fill=(255, 214, 102, 255))
    image.alpha_composite(text)
    return image


def main(ico, png):
    image = draw()
    image.save(ico, sizes=[(n, n) for n in (16, 24, 32, 48, 64, 128, 256)])
    image.resize((256, 256), Image.LANCZOS).save(png)


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
