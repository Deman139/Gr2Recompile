"""Makes the Killzone Shadow Fall icon (.ico, every Windows size) from the game's own cover
(sce_sys/icon0.png of the player's dump). Needs Pillow.
usage: make_kzsf_icon.py <icon0.png> <out.ico>"""
import sys
from PIL import Image, ImageDraw

SIZES = (16, 24, 32, 48, 64, 128, 256)


def main(source, target):
    with Image.open(source) as cover:
        cover = cover.convert('RGBA').resize((1024, 1024), Image.LANCZOS)
    mask = Image.new('L', cover.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((16, 16, 1008, 1008), radius=150, fill=255)
    icon = Image.new('RGBA', cover.size, (0, 0, 0, 0))
    icon.paste(cover, (0, 0), mask)
    icon.save(target, sizes=[(s, s) for s in SIZES])
    print(f'{target}: {", ".join(map(str, SIZES))} px')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
