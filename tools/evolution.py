"""Wardrobe for the desk cat (the HD bongo loaf). Items unlock as the cat evolves and can each
be switched on or off on the phone page:

    Lv 5  : red sweatband across the forehead, knot tails flying off the back
    Lv 15 : gaming headset (and the cat gets fluffier: cheek and head tufts, not optional)
    Lv 30 : tilted gold crown, a red cape tied under the chin, a rainbow keyboard

Each item is its own coat-independent layer; the cape is two (cloth behind the body, tie in front).
The fluff tufts are per coat and drawn just before the body, so the body's outline stays
crisp over their roots. Everything is on the 128 HD grid, like the rest of the desk cat.
"""
import colorsys
from PIL import Image, ImageDraw, ImageFilter

N = 128
INK = (32, 40, 56)


def _hex(h):
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def _outlined(mask, fill_fn, clip_inside=None):
    """paint a mask with fill_fn(x, y) and a 1 px ink outline; outline pixels that land on the
    cat (clip_inside[x][y] truthy) are skipped so gear lying on the fur has no black line"""
    im = Image.new("RGBA", (N, N))
    p = im.load()
    mp = mask.load()
    ol = mask.filter(ImageFilter.MaxFilter(3)).load()
    for y in range(N):
        for x in range(N):
            if mp[x, y]:
                p[x, y] = fill_fn(x, y, mp) + (255,)
            elif ol[x, y] and not (clip_inside and clip_inside(x, y)):
                p[x, y] = INK + (255,)
    return im


def _banded(base, shade, hi):
    """colour a gear piece: highlight on its top row, shade on its bottom row"""
    def f(x, y, mp):
        top = y == 0 or not mp[x, y - 1]
        bot = y == N - 1 or not mp[x, y + 1]
        return _hex(shade if bot else (hi if top else base))
    return f


SS = 4  # supersampling for the shapes drawn at an angle


def _ss_mask(draw_fn):
    """draw at 4x and threshold back down: clean pixel edges for tilted / curved shapes"""
    big = Image.new("L", (N * SS, N * SS), 0)
    draw_fn(ImageDraw.Draw(big), lambda pts: [(x * SS, y * SS) for x, y in pts])
    small = big.resize((N, N), Image.BOX)
    return small.point(lambda v: 255 if v >= 128 else 0)


def _rot(pts, ang, ox, oy):
    """rotate local points (around 0,0) by ang degrees clockwise and move them to ox, oy"""
    import math
    a = math.radians(ang)
    ca, sa = math.cos(a), math.sin(a)
    return [(ox + x * ca - y * sa, oy + x * sa + y * ca) for x, y in pts]


def _on_cat(pc):
    body = pc.thin_outline(pc.G["body"])
    return body, (lambda x, y: 0 <= x < N and 0 <= y < N and body[y][x] in ("fur", "line"))


def w_band(pc):
    """sweatband: 4 rows parallel to the eye line, 9 px above the eyes, knot tails off the back"""
    body, on_cat = _on_cat(pc)
    out = Image.new("RGBA", (N, N))
    (lx, ly), (rx, ry) = pc.EYE_L, pc.EYE_R
    slope = (ry - ly) / (rx - lx)
    band = Image.new("L", (N, N), 0)
    bp = band.load()
    for x in range(N):
        yc = ly - 9 + (x - lx) * slope
        for y in range(int(round(yc)) - 2, int(round(yc)) + 2):
            if 0 <= y < N and body[y][x] == "fur" and y < 66:
                bp[x, y] = 255
    out.alpha_composite(_outlined(band, _banded("E84858", "B83048", "F88898"), on_cat))
    xs = [x for x in range(N) for y in range(N) if bp[x, y]]
    kx = max(xs)
    ky = int(round(ly - 9 + (kx - lx) * slope))
    knot = Image.new("L", (N, N), 0)
    kd = ImageDraw.Draw(knot)
    kd.line([(kx - 2, ky - 1), (kx + 9, ky - 7)], fill=255, width=3)
    kd.line([(kx - 2, ky + 1), (kx + 8, ky + 7)], fill=255, width=3)
    out.alpha_composite(_outlined(knot, _banded("E84858", "B83048", "F88898"), on_cat))
    return out


# headset colours: slate shell, lighter top edge, blue LED ring on the cups
HS = ("4A5070", "343850", "7A84A8")


def w_headset(pc):
    """gaming headset that sits ON the head: the band hugs the top of the head (just inside the
    outline, in front of the ear bases), round cups on both sides of the head below the ears,
    and a mic boom from the left cup toward the mouth"""
    out = Image.new("RGBA", (N, N))
    path = [(33, 47), (37, 43.5), (43, 40.5), (50, 39), (58, 38.5), (66, 39), (74, 41.5), (84, 44.5), (94, 47.5), (104, 52)]
    band = _ss_mask(lambda d, s: d.line(s(path), fill=255, width=2 * SS + 2, joint="curve"))
    out.alpha_composite(_outlined(band, _banded(*HS)))
    cups = _ss_mask(lambda d, s: (d.ellipse(s([(28.5, 46), (38.5, 58)]), fill=255),
                                  d.ellipse(s([(100, 50), (110.5, 63)]), fill=255)))

    def cup_col(x, y, mp):
        edge = any(not mp[x + a, y + b] for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1)) if 0 <= x + a < N and 0 <= y + b < N)
        inner = all(mp[x + a, y + b] for a in (-2, 0, 2) for b in (-2, 0, 2) if 0 <= x + a < N and 0 <= y + b < N)
        if edge:
            return _hex(HS[1])
        return _hex("58A0F0") if not inner else _hex(HS[0])  # LED ring just inside the rim
    out.alpha_composite(_outlined(cups, cup_col))
    mic = _ss_mask(lambda d, s: (d.line(s([(35, 57), (39, 61.5), (44, 63.5)]), fill=255, width=SS + 2, joint="curve"),
                                 d.ellipse(s([(42.5, 61.5), (47, 66)]), fill=255)))
    out.alpha_composite(_outlined(mic, lambda x, y, mp: _hex("26262F" if x < 43 else "E84858")))
    return out


CROWN = [(-8, 0), (-9, -11), (-4.5, -6), (0, -14), (4.5, -6), (9, -11), (8, 0)]


def w_crown(pc, ang=24, at=(83, 46)):
    """gold crown tilted at a jaunty angle, sitting on the top of the head between the ears"""
    m = _ss_mask(lambda d, s: d.polygon(s(_rot(CROWN, ang, *at)), fill=255))
    jewels = {tuple(int(round(v)) for v in pt) for pt in _rot([(0, -4), (-5, -3), (5, -3)], ang, *at)}

    def col(x, y, mp):
        if (x, y) in jewels:
            return _hex("E84858")
        return _banded("F8C038", "D09020", "FFF0A0")(x, y, mp)
    return _outlined(m, col)


CAPE = ("C83040", "A02030", "E05060")


def w_cape(pc):
    """the cape's cloth, drawn BEHIND the body: it hangs off the cat's back on the right and
    shows past the shoulder"""
    m = _ss_mask(lambda d, s: d.polygon(s([(92, 54), (108, 56), (121, 70), (126, 100), (96, 100)]), fill=255))
    body, on_cat = _on_cat(pc)

    def col(x, y, mp):  # vertical folds
        return _hex(CAPE[1] if (x + y // 6) % 7 == 0 else CAPE[0])
    return _outlined(m, col)


def w_tie(pc):
    """the cape's collar tied under the chin: a red strip along the jaw (parallel to the eyes),
    a knot with two short tails, and a gold clasp"""
    (lx, ly), (rx, ry) = pc.EYE_L, pc.EYE_R
    slope = (ry - ly) / (rx - lx)
    y_at = lambda x, off: ly + off + (x - lx) * slope
    body, on_cat = _on_cat(pc)
    kx = 60
    strip = _ss_mask(lambda d, s: d.line(s([(40, y_at(40, 11.5)), (kx, y_at(kx, 13)), (96, y_at(96, 12))]), fill=255, width=3 * SS))
    out = _outlined(strip, _banded(*CAPE))
    ky = y_at(kx, 13)
    tails = _ss_mask(lambda d, s: (d.polygon(s([(kx - 1, ky), (kx - 7, ky + 8), (kx - 3, ky + 9)]), fill=255),
                                   d.polygon(s([(kx + 1, ky), (kx + 7, ky + 7), (kx + 3, ky + 9)]), fill=255)))
    out.alpha_composite(_outlined(tails, _banded(*CAPE)))
    clasp = _ss_mask(lambda d, s: d.ellipse(s([(kx - 2.5, ky - 2.5), (kx + 2.5, ky + 2.5)]), fill=255))
    out.alpha_composite(_outlined(clasp, lambda x, y, mp: _hex("FFF0A0" if (x, y) == (kx - 1, int(ky) - 1) else "F8C038")))
    return out


WARDROBE = ["w_cape", "w_band", "w_headset", "w_crown", "w_tie"]


def wardrobe(pc):
    return {"w_cape": w_cape(pc), "w_band": w_band(pc), "w_headset": w_headset(pc),
            "w_crown": w_crown(pc), "w_tie": w_tie(pc)}


def fluff(coat, pc):
    """fur tufts on the cheek and head (stage 3+), coat coloured with the coat's outline"""
    c = pc.COAT[coat]
    m = Image.new("L", (N, N), 0)
    d = ImageDraw.Draw(m)
    for tri in ([(28, 48), (18, 51), (27, 54)], [(27, 54), (17, 59), (28, 59)],    # cheek
                [(66, 38), (68, 30), (72, 37)], [(72, 39), (76, 32), (78, 40)]):    # head
        d.polygon(tri, fill=255)
    im = Image.new("RGBA", (N, N))
    p = im.load()
    mp = m.load()
    ol = m.filter(ImageFilter.MaxFilter(3)).load()
    for y in range(N):
        for x in range(N):
            if mp[x, y]:
                p[x, y] = pc.C(c["base"] if coat != "tuxedo" else c["base"]) + (255,)
            elif ol[x, y]:
                p[x, y] = pc.C(c["line"]) + (255,)
    return im


def desk_rgb(desk):
    """the desk with a rainbow keyboard (stage 4): light key pixels tinted by position"""
    im = desk.copy()
    p = im.load()
    for y in range(N):
        for x in range(N):
            r, g, b, a = p[x, y]
            if a and (r, g, b) in ((0xCD, 0xD0, 0xDA), (0x8E, 0x92, 0xA3)):
                hue = ((x * 0.9 + y * 0.4) % 128) / 128
                light = 0.72 if r == 0xCD else 0.5
                rr, gg, bb = colorsys.hls_to_rgb(hue, light, 0.75)
                p[x, y] = (int(rr * 255), int(gg * 255), int(bb * 255), 255)
    return im
