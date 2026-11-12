#!/usr/bin/env python3
"""Draw every cat (3 cats x 3 coats x 2 styles, plus the original 8-bit cat) and
export them for the firmware and the config page.

    .venv/bin/python tools/make_cats.py            # previews only (tools/out/)
    .venv/bin/python tools/make_cats.py --export   # + firmware/src/cat_assets.{h,cpp} and app previews

Everything is drawn in a 240 x 200 "cat area" (the bottom of the screen, y 120..320):
the cat sits behind a desk with a keyboard, paws on the keys. Layers, back to front:
body -> face -> desk -> paws -> effect. The firmware composites them over the weather.

Styles
  cartoon : smooth shapes, 4x supersampled for anti-aliased edges, soft shading
  pixel   : the same shapes drawn hard-edged at half resolution with 3-tone shading,
            shown at 2x on the board (twice the detail of the original 8-bit cat)
  8bit    : the original 64x64 sprites from animations/, shown at 3x
"""
import math, os, re, sys
from PIL import Image, ImageDraw, ImageFilter, ImageChops

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "tools", "out")
AW, AH = 240, 200          # cat area in design units (= screen pixels)
DESK_Y = 146               # desk top edge

CATS = ["classic", "chonky", "kitten"]
COATS = ["default", "grey", "tabby"]
STYLES = ["cartoon", "pixel"]

COAT = {  # fur light, fur dark (shading), muzzle/belly, outline, inner ear, stripe
    "default": dict(fur=(255, 255, 255), shade=(226, 222, 216), muzzle=(255, 255, 255), line=(46, 42, 40), ear=(246, 178, 190), stripe=None),
    "grey":    dict(fur=(176, 181, 189), shade=(136, 142, 151), muzzle=(222, 225, 229), line=(52, 55, 61), ear=(232, 170, 182), stripe=(110, 115, 124)),
    "tabby":   dict(fur=(240, 170, 98), shade=(214, 136, 66), muzzle=(250, 226, 198), line=(78, 46, 26), ear=(244, 170, 170), stripe=(178, 98, 42)),
}
INK = (40, 34, 34)
PINK = (238, 120, 140)
BLUSH = (255, 140, 160)

# Per-cat proportions (design units). body: centre x, top y, half width, bottom y (hidden by desk)
SHAPE = {
    "classic": dict(cx=120, top=40, hw=84, ear_w=30, ear_h=36, eye_y=96, eye_dx=26, eye_r=(6, 8), nose_y=110, cheek_dx=46),
    "chonky":  dict(cx=120, top=52, hw=100, ear_w=26, ear_h=28, eye_y=102, eye_dx=28, eye_r=(6, 7), nose_y=115, cheek_dx=54),
    "kitten":  dict(cx=120, top=56, hw=70, ear_w=30, ear_h=40, eye_y=104, eye_dx=24, eye_r=(8, 10), nose_y=118, cheek_dx=40),
}
PAW_X = {"classic": (76, 164), "chonky": (70, 170), "kitten": (84, 156)}


class Canvas:
    """Draws in design units onto a scaled image. cartoon: k=4 (supersampled, shrunk at the end);
    pixel: k=0.5 (hard pixels)."""

    def __init__(self, style):
        self.style = style
        self.k = 4 if style == "cartoon" else 0.5
        self.w, self.h = round(AW * self.k), round(AH * self.k)

    def new(self, mode="RGBA"):
        return Image.new(mode, (self.w, self.h), 0)

    def p(self, *xy):
        return [v * self.k for v in xy]

    def finish(self, img):
        if self.style == "cartoon":
            return img.resize((AW, AH), Image.LANCZOS)
        return img

    def line_w(self, design_px):
        return max(1, round(design_px * self.k))


def body_mask(cv, s, twitch=False):
    m = cv.new("L")
    d = ImageDraw.Draw(m)
    cx, top, hw = s["cx"], s["top"], s["hw"]
    bottom = 230  # well below the desk
    # loaf: big rounded rectangle with a very round top
    r = hw * 0.9
    d.rounded_rectangle(cv.p(cx - hw, top, cx + hw, bottom), radius=r * cv.k, fill=255)
    # ears
    for side in (-1, 1):
        ex = cx + side * (hw * 0.58)
        ew, eh = s["ear_w"], s["ear_h"]
        tip_dx = side * ew * 0.25
        if twitch and side == 1:
            tip_dx += 8
            eh -= 6
        pts = [(ex - ew, top + 22), (ex + tip_dx, top - eh + 14), (ex + ew, top + 16)]
        d.polygon([tuple(cv.p(*pt)) for pt in pts], fill=255)
    if cv.style == "cartoon":  # soften the ear tips
        m = m.filter(ImageFilter.GaussianBlur(cv.k * 1.2)).point(lambda v: 255 if v > 128 else 0)
    return m


def inner_ears(cv, s, twitch=False):
    m = cv.new("L")
    d = ImageDraw.Draw(m)
    cx, top, hw = s["cx"], s["top"], s["hw"]
    for side in (-1, 1):
        ex = cx + side * (hw * 0.58)
        ew, eh = s["ear_w"] * 0.5, s["ear_h"] * 0.62
        tip_dx = side * ew * 0.4
        if twitch and side == 1:
            tip_dx += 7
            eh -= 5
        pts = [(ex - ew, top + 20), (ex + tip_dx, top - eh + 14 + 4), (ex + ew, top + 16)]
        d.polygon([tuple(cv.p(*pt)) for pt in pts], fill=255)
    return m


def dilate(cv, mask, design_px):
    n = max(1, round(design_px * cv.k))
    size = 2 * n + 1
    out = mask
    # MaxFilter sizes must be odd; apply in chunks to keep it fast
    while size > 1:
        step = min(size, 9)
        if step % 2 == 0:
            step -= 1
        out = out.filter(ImageFilter.MaxFilter(step))
        size -= step - 1
    return out


def shaded_fill(cv, mask, c, top, bottom):
    """Fur: light at the top, shade colour towards the bottom; pixel style uses 3 bands."""
    h = cv.h
    grad = Image.new("RGBA", (cv.w, h))
    gd = ImageDraw.Draw(grad)
    t0, t1 = top * cv.k, bottom * cv.k
    for y in range(h):
        t = min(1, max(0, (y - t0) / max(1, t1 - t0)))
        t = t ** 1.6
        if cv.style == "pixel":
            t = 0 if t < 0.45 else (0.5 if t < 0.8 else 1)
        col = tuple(round(a + (b - a) * t) for a, b in zip(c["fur"], c["shade"])) + (255,)
        gd.line([(0, y), (cv.w, y)], fill=col)
    out = cv.new()
    out.paste(grad, (0, 0), mask)
    return out


def stripes(cv, s, c, mask):
    if not c["stripe"]:
        return None
    m = cv.new("L")
    d = ImageDraw.Draw(m)
    cx, top, hw = s["cx"], s["top"], s["hw"]
    w = cv.line_w(5)
    # forehead "M"
    for dx in (-12, 0, 12):
        d.line([tuple(cv.p(cx + dx, top + 6)), tuple(cv.p(cx + dx * 0.8, top + 26))], fill=255, width=w)
    # side stripes
    for side in (-1, 1):
        for i, yy in enumerate((top + 52, top + 72, top + 92)):
            x0 = cx + side * (hw + 4)
            x1 = cx + side * (hw - 22 - i * 2)
            d.line([tuple(cv.p(x0, yy)), tuple(cv.p(x1, yy + 6))], fill=255, width=w)
    m = ImageChops.multiply(m, mask)
    out = cv.new()
    out.paste(Image.new("RGBA", (cv.w, cv.h), c["stripe"] + (255,)), (0, 0), m)
    return out


def draw_body(style, cat, coat, twitch=False):
    cv = Canvas(style)
    s, c = SHAPE[cat], COAT[coat]
    mask = body_mask(cv, s, twitch)
    ol = dilate(cv, mask, 3 if style == "cartoon" else 2)
    img = cv.new()
    img.paste(Image.new("RGBA", (cv.w, cv.h), c["line"] + (255,)), (0, 0), ol)
    img.alpha_composite(shaded_fill(cv, mask, c, s["top"], DESK_Y + 10))
    st = stripes(cv, s, c, mask)
    if st:
        img.alpha_composite(st)
    # muzzle / cheeks patch
    mz = cv.new("L")
    d = ImageDraw.Draw(mz)
    ny = s["nose_y"]
    d.ellipse(cv.p(s["cx"] - 26, ny - 8, s["cx"] + 26, ny + 22), fill=255)
    mzc = cv.new()
    mzc.paste(Image.new("RGBA", (cv.w, cv.h), c["muzzle"] + (255,)), (0, 0), ImageChops.multiply(mz, mask))
    img.alpha_composite(mzc)
    # inner ears
    ie = ImageChops.multiply(inner_ears(cv, s, twitch), mask)
    iec = cv.new()
    iec.paste(Image.new("RGBA", (cv.w, cv.h), c["ear"] + (255,)), (0, 0), ie)
    img.alpha_composite(iec)
    return cv.finish(img)


def draw_face(style, cat, kind):
    """Eyes, nose, mouth, blush. Coat-independent."""
    cv = Canvas(style)
    s = SHAPE[cat]
    img = cv.new()
    d = ImageDraw.Draw(img)
    cx, ey, edx = s["cx"], s["eye_y"], s["eye_dx"]
    rx, ry = s["eye_r"]
    lw = cv.line_w(3 if style == "cartoon" else 2)
    for side in (-1, 1):
        ex = cx + side * edx
        if kind == "stock":
            d.ellipse(cv.p(ex - rx, ey - ry, ex + rx, ey + ry), fill=INK + (255,))
            hr = max(1.5, rx * 0.35)
            d.ellipse(cv.p(ex - rx * 0.4 - hr, ey - ry * 0.45 - hr, ex - rx * 0.4 + hr, ey - ry * 0.45 + hr), fill=(255, 255, 255, 255))
        elif kind == "happy":  # ^ ^
            d.line([tuple(cv.p(ex - rx - 1, ey + 2)), tuple(cv.p(ex, ey - ry + 1)), tuple(cv.p(ex + rx + 1, ey + 2))],
                   fill=INK + (255,), width=lw, joint="curve")
        elif kind == "blink":
            d.line([tuple(cv.p(ex - rx - 1, ey)), tuple(cv.p(ex + rx + 1, ey))], fill=INK + (255,), width=lw)
        elif kind == "sleepy":  # closed, curved down
            d.arc(cv.p(ex - rx - 1, ey - ry, ex + rx + 1, ey + ry * 0.6), 20, 160, fill=INK + (255,), width=lw)
    # blush
    for side in (-1, 1):
        bx = cx + side * s["cheek_dx"]
        a = 150 if kind == "happy" else 90
        d.ellipse(cv.p(bx - 10, s["nose_y"] - 2, bx + 10, s["nose_y"] + 8), fill=BLUSH + (a,))
    # nose + mouth
    ny = s["nose_y"]
    d.polygon([tuple(cv.p(cx - 5, ny)), tuple(cv.p(cx + 5, ny)), tuple(cv.p(cx, ny + 5))], fill=PINK + (255,))
    mw = cv.line_w(2.2 if style == "cartoon" else 2)
    if kind == "happy":
        d.chord(cv.p(cx - 8, ny + 3, cx + 8, ny + 17), 0, 180, fill=(170, 60, 70, 255), outline=INK + (255,), width=mw)
    else:
        d.arc(cv.p(cx - 9, ny + 1, cx, ny + 11), 0, 160, fill=INK + (255,), width=mw)
        d.arc(cv.p(cx, ny + 1, cx + 9, ny + 11), 20, 180, fill=INK + (255,), width=mw)
    return cv.finish(img)


def desk_and_keyboard(style):
    cv = Canvas(style)
    img = cv.new()
    d = ImageDraw.Draw(img)
    wood, wood_d = (176, 120, 78), (140, 92, 58)
    d.rectangle(cv.p(0, DESK_Y, AW, AH), fill=wood + (255,))
    d.rectangle(cv.p(0, DESK_Y, AW, DESK_Y + 4), fill=(200, 146, 102, 255))
    for i, y in enumerate(range(DESK_Y + 14, AH, 14)):  # grain
        d.line([tuple(cv.p(10 + (i * 37) % 60, y)), tuple(cv.p(90 + (i * 53) % 90, y))], fill=wood_d + (255,), width=cv.line_w(1.5))
    # keyboard
    kx0, ky0, kx1, ky1 = 34, DESK_Y + 6, 206, DESK_Y + 44
    d.rounded_rectangle(cv.p(kx0, ky0 + 3, kx1, ky1 + 3), radius=6 * cv.k, fill=(40, 40, 46, 255))  # shadow lip
    d.rounded_rectangle(cv.p(kx0, ky0, kx1, ky1), radius=6 * cv.k, fill=(70, 72, 82, 255), outline=(36, 36, 42, 255), width=cv.line_w(2))
    cols = 12 if style == "cartoon" else 8   # half-res pixel keys need to be bigger to stay distinct
    for r in range(3):
        n = cols - (1 if r == 2 else 0)
        kw = (kx1 - kx0 - 12) / cols
        off = kx0 + 6 + (kw / 2 if r == 1 else 0)
        for i in range(n):
            x = off + i * kw
            y = ky0 + 5 + r * 11
            d.rounded_rectangle(cv.p(x + 1, y, x + kw - 1, y + 8), radius=2 * cv.k, fill=(214, 216, 224, 255))
    return cv.finish(img)


def paw(cv, d, c, x, y, down):
    """One paw: rounded fur blob with outline and toe lines."""
    w, h = (26, 20) if not down else (28, 18)
    m = cv.new("L")
    ImageDraw.Draw(m).ellipse(cv.p(x - w / 2, y - h / 2, x + w / 2, y + h / 2), fill=255)
    ol = dilate(cv, m, 3 if cv.style == "cartoon" else 2)
    layer = cv.new()
    layer.paste(Image.new("RGBA", (cv.w, cv.h), c["line"] + (255,)), (0, 0), ol)
    layer.paste(Image.new("RGBA", (cv.w, cv.h), (c["muzzle"] if c["stripe"] else c["fur"]) + (255,)), (0, 0), m)
    ld = ImageDraw.Draw(layer)
    for dx in (-5, 0, 5):
        ld.line([tuple(cv.p(x + dx, y + h / 2 - 6)), tuple(cv.p(x + dx, y + h / 2 - 2))], fill=c["line"] + (255,), width=cv.line_w(1.6))
    return layer


def draw_paws(style, cat, coat, which):
    """which: 'up' (both raised), 'left' (left pressing), 'right' (right pressing)."""
    cv = Canvas(style)
    c = COAT[coat]
    img = cv.new()
    lx, rx = PAW_X[cat]
    up_y, down_y = DESK_Y - 8, DESK_Y + 14
    for x, side in ((lx, "left"), (rx, "right")):
        down = which == side
        img.alpha_composite(paw(cv, None, c, x, down_y if down else up_y, down))
    return cv.finish(img)


def draw_effect(style, cat, kind, frame=0):
    cv = Canvas(style)
    img = cv.new()
    d = ImageDraw.Draw(img)
    lw = cv.line_w(3 if style == "cartoon" else 2)
    if kind in ("left", "right"):
        lx, rx = PAW_X[cat]
        x = lx if kind == "left" else rx
        y = DESK_Y + 18
        col = (255, 214, 64, 255)
        for a in (-150, -110, -70, -30):
            r0, r1 = 20, 30
            ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
            d.line([tuple(cv.p(x + ca * r0, y + sa * r0)), tuple(cv.p(x + ca * r1, y + sa * r1))], fill=col, width=lw)
    else:  # zzz: three z's rising, frame 0..2 shifts them
        s = SHAPE[cat]
        base_x, base_y = min(s["cx"] + s["hw"] - 26, 178), s["top"] + 8
        col = (120, 170, 255, 255)
        for i in range(3):
            j = (i + frame) % 3
            size = 9 + j * 4
            x = base_x + j * 14
            y = base_y - j * 18
            pts = [(x, y), (x + size, y), (x, y + size), (x + size, y + size)]
            d.line([tuple(cv.p(*p)) for p in pts], fill=col, width=lw, joint="curve")
    return cv.finish(img)


# ------------------------------------------------------------------ original 8-bit sprites
def load_8bit(rel):
    txt = open(os.path.join(ROOT, "animations", rel)).read()
    body = txt.split("_map[] = {", 1)[1].split("};", 1)[0]
    b = [int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", body)]
    img = Image.new("RGBA", (64, 64))
    for i in range(64 * 64):
        c = b[2 * i] | (b[2 * i + 1] << 8)
        r, g, bl = (c >> 11) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31
        img.putpixel((i % 64, i // 64), (r, g, bl, b[64 * 64 * 2 + i]))
    return img


EIGHT = {  # layer name -> sprite file
    "body": "body/standardbody1.c", "body_twitch": "body/bodyeartwitch.c",
    "face_stock": "faces/stock_face.c", "face_happy": "faces/happy_face.c",
    "face_blink": "faces/blink_face.c", "face_sleepy": "faces/sleepy_face.c",
    "desk": "table/table1.c",
    "paws_up": "paws/twopawsup.c", "paws_left": "paws/leftpawdown.c", "paws_right": "paws/rightpawdown.c",
    "fx_left": "effects/left_click_effect.c", "fx_right": "effects/right_click_effect.c",
    "fx_zzz0": "effects/sleepy1.c", "fx_zzz1": "effects/sleepy2.c", "fx_zzz2": "effects/sleepy3.c",
}


# ------------------------------------------------------------------ build all layers
def all_layers():
    """Yields (key, image, scale). key like 'cartoon/classic/default/body'.
    scale = how much the firmware enlarges it (pixel art is stored at half size)."""
    for style in STYLES:
        sc = 2 if style == "pixel" else 1
        for k, img, _ in _style_layers(style):
            yield k, img, sc
    for name, rel in EIGHT.items():
        yield f"8bit/-/-/{name}", load_8bit(rel), 3


def _style_layers(style):
    if True:
        for cat in CATS:
            for coat in COATS:
                yield f"{style}/{cat}/{coat}/body", draw_body(style, cat, coat), 1
                yield f"{style}/{cat}/{coat}/body_twitch", draw_body(style, cat, coat, twitch=True), 1
                for which in ("up", "left", "right"):
                    yield f"{style}/{cat}/{coat}/paws_{which}", draw_paws(style, cat, coat, which), 1
            for kind in ("stock", "happy", "blink", "sleepy"):
                yield f"{style}/{cat}/-/face_{kind}", draw_face(style, cat, kind), 1
            for kind in ("left", "right"):
                yield f"{style}/{cat}/-/fx_{kind}", draw_effect(style, cat, kind), 1
            for f in range(3):
                yield f"{style}/{cat}/-/fx_zzz{f}", draw_effect(style, cat, "zzz", f), 1
        yield f"{style}/-/-/desk", desk_and_keyboard(style), 1


def compose(layers, style, cat, coat, face="stock", paws="up", fx=None, twitch=False, bg=(150, 190, 230)):
    """Full cat area at screen resolution, for previews."""
    def get(name):
        for k in (f"{style}/{cat}/{coat}/{name}", f"{style}/{cat}/-/{name}", f"{style}/-/-/{name}"):
            if k in layers:
                img, sc = layers[k]
                return img.resize((img.width * sc, img.height * sc), Image.NEAREST) if sc > 1 else img
        return None
    out = Image.new("RGBA", (AW, AH), bg + (255,))

    def put(im):
        if im is None:
            return
        if style == "8bit":  # 64*3=192 wide: centre it, bottom aligned
            out.alpha_composite(im, ((AW - im.width) // 2, AH - im.height))
        else:
            out.alpha_composite(im)
    put(get("body_twitch" if twitch else "body"))
    put(get(f"face_{face}"))
    put(get("desk"))
    put(get(f"paws_{paws}"))
    if fx:
        put(get(f"fx_{fx}"))
    return out


def previews(layers):
    os.makedirs(OUT, exist_ok=True)
    # contact sheet: rows = style x cat, cols = coats
    sheet = Image.new("RGBA", (AW * 3 + 40, (AH + 10) * 7), (255, 255, 255, 255))
    y = 0
    for style in STYLES:
        for cat in CATS:
            for i, coat in enumerate(COATS):
                sheet.alpha_composite(compose(layers, style, cat, coat), (i * (AW + 20), y))
            y += AH + 10
    sheet.alpha_composite(compose(layers, "8bit", "-", "-"), (0, y))
    sheet.save(os.path.join(OUT, "sheet.png"))
    # poses for one cat
    poses = Image.new("RGBA", (AW * 4 + 60, AH), (255, 255, 255, 255))
    for i, kw in enumerate([dict(face="stock", paws="up"), dict(face="happy", paws="left", fx="left"),
                            dict(face="blink", paws="right", twitch=True), dict(face="sleepy", paws=None, fx="zzz1")]):
        poses.alpha_composite(compose(layers, "cartoon", "classic", "tabby", **kw), (i * (AW + 20), 0))
    poses.save(os.path.join(OUT, "poses.png"))


if __name__ == "__main__":
    layers = {k: (img, sc) for k, img, sc in all_layers()}
    previews(layers)
    print("wrote", os.path.join(OUT, "sheet.png"), os.path.join(OUT, "poses.png"))
    if "--export" in sys.argv:
        import export_cats
        export_cats.export(layers, compose)
