"""Evolution gear for the desk cat (the HD bongo loaf), by stage:

    2 (Lv 5)  : red sweatband across the forehead, knot tails flying off the back
    3 (Lv 15) : + fluffier (cheek and head tufts) + gaming headset
    4 (Lv 30) : + gold crown and a rainbow keyboard

Gear layers are coat-independent and cumulative (gear4 includes the band and headset).
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


def gear(stage, pc):
    body = pc.thin_outline(pc.G["body"])
    on_cat = lambda x, y: 0 <= x < N and 0 <= y < N and body[y][x] in ("fur", "line")
    out = Image.new("RGBA", (N, N))

    # sweatband: 4 rows parallel to the eye line, 9 px above the eyes, clipped to the head
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
    # knot tails off the back of the head (right side), past the silhouette
    xs = [x for x in range(N) for y in range(N) if bp[x, y]]
    kx = max(xs)
    ky = int(round(ly - 9 + (kx - lx) * slope))
    knot = Image.new("L", (N, N), 0)
    kd = ImageDraw.Draw(knot)
    kd.line([(kx - 2, ky - 1), (kx + 9, ky - 7)], fill=255, width=3)
    kd.line([(kx - 2, ky + 1), (kx + 8, ky + 7)], fill=255, width=3)
    out.alpha_composite(_outlined(knot, _banded("E84858", "B83048", "F88898"), on_cat))

    if stage >= 3:  # headset: band over the head, cups at the sides, mic toward the mouth
        hs = Image.new("L", (N, N), 0)
        hd = ImageDraw.Draw(hs)
        hd.line([(30, 49), (40, 42), (52, 37), (64, 36), (78, 41), (92, 47), (104, 53)], fill=255, width=3, joint="curve")
        out.alpha_composite(_outlined(hs, _banded("3A3A4A", "26262F", "5A5A70")))
        cups = Image.new("L", (N, N), 0)
        cd = ImageDraw.Draw(cups)
        cd.ellipse([22, 47, 33, 60], fill=255)
        cd.ellipse([101, 52, 112, 65], fill=255)
        cd.line([(30, 59), (43, 66)], fill=255, width=2)
        cd.ellipse([41, 64, 46, 69], fill=255)

        def cup_col(x, y, mp):
            ring = any(not mp[x + a, y + b] for a, b in ((1, 0), (-1, 0), (0, 1), (0, -1)) if 0 <= x + a < N and 0 <= y + b < N)
            return _hex("58A0F0" if ring and y > 50 and x in range(22, 34) else ("26262F" if ring else "3A3A4A"))
        out.alpha_composite(_outlined(cups, cup_col))

    if stage >= 4:  # crown on the head between the ears
        cr = Image.new("L", (N, N), 0)
        ImageDraw.Draw(cr).polygon([(73, 43), (73, 35), (77, 38), (81, 31), (85, 38), (89, 35), (89, 45)], fill=255)

        def crown_col(x, y, mp):
            if (x, y) in ((81, 39), (76, 41), (86, 42)):
                return _hex("E84858")
            return _banded("F8C038", "D09020", "FFF0A0")(x, y, mp)
        out.alpha_composite(_outlined(cr, crown_col))
    return out


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
