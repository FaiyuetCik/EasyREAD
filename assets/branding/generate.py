"""Generate the Faiyuet Morgan monochrome identity and Sticky splash preview."""

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont


OUT = Path(__file__).resolve().parent
SCALE = 4
BLACK = 0
WHITE = 255


def font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    filename = "georgiab.ttf" if bold else "georgia.ttf"
    return ImageFont.truetype(str(Path("C:/Windows/Fonts") / filename), size)


def centered_text(draw: ImageDraw.ImageDraw, xy: tuple[int, int], value: str,
                  typeface: ImageFont.FreeTypeFont, fill: int = BLACK) -> None:
    box = draw.textbbox((0, 0), value, font=typeface)
    width = box[2] - box[0]
    draw.text((xy[0] - width // 2, xy[1] - box[1]), value, font=typeface, fill=fill)


def emblem(draw: ImageDraw.ImageDraw, x: int, y: int, size: int) -> None:
    """An open book with a raised turning page; geometry is reused in both PNGs."""
    u = size / 300
    def pts(values):
        return [(round(x + px * u), round(y + py * u)) for px, py in values]
    w = max(1, round(8 * u))
    # Lower edge suggests a book resting open, distinct at small sizes.
    draw.line(pts([(30, 214), (138, 241), (150, 251), (162, 241), (270, 214)]),
              fill=BLACK, width=w, joint="curve")
    # Left and right page outlines.
    left = [(150, 70), (114, 48), (40, 42), (40, 202), (111, 207), (150, 226)]
    right = [(150, 70), (186, 48), (260, 42), (260, 202), (189, 207), (150, 226)]
    draw.line(pts(left), fill=BLACK, width=w, joint="curve")
    draw.line(pts(right), fill=BLACK, width=w, joint="curve")
    draw.line(pts([(150, 70), (150, 226)]), fill=BLACK, width=max(1, round(7 * u)))
    # The folded upper corner reads as a page in motion.
    draw.line(pts([(199, 47), (221, 82), (260, 89)]), fill=BLACK, width=max(1, round(7 * u)))
    draw.line(pts([(221, 82), (217, 115), (250, 99)]), fill=BLACK, width=max(1, round(6 * u)))


def final_bw(image: Image.Image, size: tuple[int, int]) -> Image.Image:
    image = image.resize(size, Image.Resampling.LANCZOS)
    return image.point(lambda p: 255 if p >= 128 else 0, mode="1")


def make_logo() -> None:
    s = SCALE
    image = Image.new("L", (512 * s, 512 * s), WHITE)
    draw = ImageDraw.Draw(image)
    emblem(draw, 106 * s, 32 * s, 300 * s)
    centered_text(draw, (256 * s, 358 * s), "Faiyuet", font(49 * s, True))
    centered_text(draw, (256 * s, 419 * s), "Morgan", font(49 * s, True))
    final_bw(image, (512, 512)).save(OUT / "faiyuet-morgan-logo-512.png")


def make_splash() -> None:
    s = SCALE
    image = Image.new("L", (480 * s, 800 * s), WHITE)
    draw = ImageDraw.Draw(image)
    emblem(draw, 105 * s, 195 * s, 270 * s)
    centered_text(draw, (240 * s, 496 * s), "Faiyuet", font(53 * s, True))
    centered_text(draw, (240 * s, 559 * s), "Morgan", font(53 * s, True))
    draw.line([(183 * s, 656 * s), (297 * s, 656 * s)], fill=BLACK, width=2 * s)
    centered_text(draw, (240 * s, 675 * s), "EPUB READER", font(18 * s))
    final_bw(image, (480, 800)).save(OUT / "faiyuet-morgan-sticky-splash-480x800.png")


SVG = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512" role="img" aria-labelledby="title">
<title id="title">Faiyuet Morgan</title>
<rect width="512" height="512" fill="#fff"/>
<g fill="none" stroke="#000" stroke-linecap="round" stroke-linejoin="round">
  <path d="M136 246 L244 273 L256 283 L268 273 L376 246" stroke-width="8"/>
  <path d="M256 102 L220 80 L146 74 L146 234 L217 239 L256 258" stroke-width="8"/>
  <path d="M256 102 L292 80 L366 74 L366 234 L295 239 L256 258" stroke-width="8"/>
  <path d="M256 102 L256 258" stroke-width="7"/>
  <path d="M305 79 L327 114 L366 121 M327 114 L323 147 L356 131" stroke-width="7"/>
</g>
<text x="256" y="404" text-anchor="middle" font-family="Georgia, serif" font-weight="bold" font-size="49" fill="#000">Faiyuet</text>
<text x="256" y="465" text-anchor="middle" font-family="Georgia, serif" font-weight="bold" font-size="49" fill="#000">Morgan</text>
</svg>
'''


if __name__ == "__main__":
    make_logo()
    make_splash()
    (OUT / "faiyuet-morgan-logo.svg").write_text(SVG, encoding="utf-8")
    print("Generated logo SVG, 512px monochrome PNG, and 480x800 Sticky splash PNG")
