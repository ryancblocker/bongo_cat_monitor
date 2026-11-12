#!/usr/bin/env python3
"""HD pixel bongo cat.

The original 64x64 sprites (animations/) are upscaled with Scale2x to 128x128, which keeps
their exact shapes, then redrawn at that resolution: 1 px outline, shading from the top
right, inner ears, eye shine, blush, a proper desk + keyboard, extra animation frames
(glances, yawn, hearts) and 8 coats. The firmware shows them at 2x, the same size the
original was shown at 4x, with twice the detail.

    .venv/bin/python tools/pixelcat.py            # previews in tools/out/
    .venv/bin/python tools/pixelcat.py --export   # + firmware/src/cat_assets.* and app previews
"""
import math, os, sys
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from sprites import load_8bit, EIGHT  # noqa: E402
import heropose  # noqa: E402
import evolution  # noqa: E402

OUT = os.path.join(HERE, "out")
N = 128  # HD sprite size

COATS = ["white", "black", "grey", "orange", "calico", "tuxedo", "siamese", "cream"]
COAT_LABELS = ["White", "Black", "Grey", "Orange tabby", "Calico", "Tuxedo", "Siamese", "Cream"]

C = lambda h: tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))
COAT = {
    "white":   dict(base="FFFFFF", shade="DCDEE8", hi="FFFFFF", line="16161E", ear="F6B0BE", eyes="dark"),
    "black":   dict(base="3E3E4C", shade="2C2C38", hi="5A5A70", line="0C0C12", ear="C7899A", eyes="gold"),
    "grey":    dict(base="B3B8C6", shade="9298A9", hi="CED2DC", line="1E1F27", ear="EEA6B5", eyes="dark"),
    "orange":  dict(base="F6A754", shade="DC8636", hi="FFC57F", line="3A2210", ear="F5A7AE", eyes="dark", stripe="C1672B"),
    "calico":  dict(base="FFFFFF", shade="DCDEE8", hi="FFFFFF", line="16161E", ear="F6B0BE", eyes="dark",
                    patchA="F2A24E", patchAs="D8852F", patchB="3E3E4C", patchBs="2C2C38"),
    "tuxedo":  dict(base="30303C", shade="23232D", hi="4A4A5E", line="0C0C12", ear="C7899A", eyes="gold", bib="FFFFFF", bibs="DCDEE8"),
    "siamese": dict(base="F4EAD7", shade="E1D0B3", hi="FFF8EC", line="2A1C14", ear="5B4032", eyes="blue", point="5B4032", points="45301F"),
    "cream":   dict(base="FFF1D9", shade="F0D9B4", hi="FFF9EE", line="2B2116", ear="F6B0BE", eyes="dark"),
}
EYES = {  # iris, glint
    "dark": ("16161E", "FFFFFF"),
    "gold": ("F2C94C", "FFFFFF"),
    "blue": ("4F9BE8", "FFFFFF"),
}
PINK = C("F58FA8")
BLUSH = C("FFA3B8")


# ------------------------------------------------------------------ helpers
def scale2x(im):
    w, h = im.size
    src = im.load()
    out = Image.new("RGBA", (w * 2, h * 2))
    o = out.load()
    g = lambda x, y: src[min(w - 1, max(0, x)), min(h - 1, max(0, y))]
    for y in range(h):
        for x in range(w):
            P = src[x, y]; A = g(x, y - 1); B = g(x + 1, y); Cc = g(x - 1, y); D = g(x, y + 1)
            o[2 * x, 2 * y] = A if (Cc == A and Cc != D and A != B) else P
            o[2 * x + 1, 2 * y] = B if (A == B and A != Cc and B != D) else P
            o[2 * x, 2 * y + 1] = Cc if (D == Cc and D != B and Cc != A) else P
            o[2 * x + 1, 2 * y + 1] = D if (B == D and B != A and D != Cc) else P
    return out


def kind(px):
    """classify an original sprite pixel: '', 'line', 'fur', 'pink', 'grey'"""
    r, g, b, a = px
    if a < 128:
        return ""
    if r < 70 and g < 70 and b < 70:
        return "line"
    if r > 200 and g < 170:
        return "pink"
    if r > 200 and g > 200 and b > 200:
        return "fur"
    return "grey"


def classify(im):
    p = im.load()
    return [[kind(p[x, y]) for x in range(im.width)] for y in range(im.height)]


def at(grid, x, y):
    return grid[y][x] if 0 <= x < N and 0 <= y < N else ""


def thin_outline(grid):
    """2 px outlines -> 1 px: drop outline pixels with no fur/pink next to them (4-neighbours)."""
    out = [row[:] for row in grid]
    for y in range(N):
        for x in range(N):
            if grid[y][x] == "line":
                if not any(at(grid, x + dx, y + dy) in ("fur", "pink") for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                    out[y][x] = ""
    return out


def blobs(grid, k):
    """connected components (4-way) of cells of kind k -> list of pixel lists"""
    seen = set()
    res = []
    for y in range(N):
        for x in range(N):
            if grid[y][x] == k and (x, y) not in seen:
                stack, comp = [(x, y)], []
                seen.add((x, y))
                while stack:
                    cx, cy = stack.pop()
                    comp.append((cx, cy))
                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        nx, ny = cx + dx, cy + dy
                        if at(grid, nx, ny) == k and (nx, ny) not in seen:
                            seen.add((nx, ny))
                            stack.append((nx, ny))
                res.append(comp)
    return res


# ------------------------------------------------------------------ geometry from the originals
RAW = {k: scale2x(load_8bit(v)) for k, v in EIGHT.items()}
G = {k: classify(v) for k, v in RAW.items()}


def eye_blobs(face="face_stock"):
    bl = sorted([b for b in blobs(G[face], "line") if len(b) >= 30], key=lambda b: min(p[0] for p in b))
    return bl  # eyes are the two round 48 px blobs; the mouth pieces are smaller


def centre(b):
    return (sum(p[0] for p in b) / len(b), sum(p[1] for p in b) / len(b))


EYE_L, EYE_R = [centre(b) for b in eye_blobs()[:2]] if len(eye_blobs()) >= 2 else ((40, 50), (60, 60))
# ear tips: topmost body outline pixels in the left and right halves of the head
_body_line = [(x, y) for y in range(N) for x in range(N) if G["body"][y][x] == "line"]
EAR_L = min((p for p in _body_line if p[0] < 80), key=lambda p: p[1])   # big ear over the face
EAR_R = min((p for p in _body_line if p[0] > 95), key=lambda p: p[1])   # small ear at the back
FACE_C = ((EYE_L[0] + EYE_R[0]) / 2, (EYE_L[1] + EYE_R[1]) / 2)


def fit_table():
    """least-squares straight line through the original table edge (HD coordinates)"""
    g64 = classify(load_8bit(EIGHT["desk"]))
    at64 = lambda x, y: g64[y][x] if 0 <= x < 64 and 0 <= y < 64 else ""
    edge = [(2 * x + 1, 2 * y + 1) for y in range(64) for x in range(64)
            if g64[y][x] == "line" and not any(at64(x + dx, y + dy) == "grey" for dx in (-2, -1, 0, 1, 2) for dy in (-2, -1, 0, 1, 2))]
    xs = [q[0] for q in edge]; ys = [q[1] for q in edge]
    mx, my = sum(xs) / len(xs), sum(ys) / len(ys)
    a = sum((x - mx) * (y - my) for x, y in zip(xs, ys)) / sum((x - mx) ** 2 for x in xs)
    return a, my - a * mx


TABLE = fit_table()


# ------------------------------------------------------------------ painting
def fur_color(coat, x, y, grid):
    """base / shade / highlight + coat pattern for a fur pixel"""
    c = COAT[coat]
    # shading: light from the top right; fur with outline/outside just below-left is in shadow
    shade = any(at(grid, x - k, y + k) in ("line", "") for k in (1, 2, 3))
    hi = coat != "white" and any(at(grid, x + k, y - k) in ("line", "") for k in (1,))
    base, sh = c["base"], c["shade"]

    dx, dy = x - FACE_C[0], y - FACE_C[1]
    if coat == "orange":  # tabby stripes over the back + forehead "M"
        if y < FACE_C[1] + 6 and ((x + 0.55 * y) % 9) < 2.2 and abs(dx) > 14:
            base, sh = c["stripe"], c["stripe"]
        if -6 < dx < 6 and -14 < dy < -6 and int(dx + 6) % 4 == 0:
            base, sh = c["stripe"], c["stripe"]
    elif coat == "calico":
        if (x - EAR_L[0] - 4) ** 2 / 90 + (y - EAR_L[1] - 9) ** 2 / 40 < 1:  # orange patch over the left ear
            base, sh = c["patchA"], c["patchAs"]
        elif (x - EAR_R[0] + 6) ** 2 / 70 + (y - EAR_R[1] - 8) ** 2 / 50 < 1:  # black patch over the right ear
            base, sh = c["patchB"], c["patchBs"]
        elif (x - FACE_C[0] - 34) ** 2 / 120 + (y - FACE_C[1] - 16) ** 2 / 60 < 1:  # orange on the back
            base, sh = c["patchA"], c["patchAs"]
    elif coat == "tuxedo":  # white muzzle + chest
        if (dx / 15) ** 2 + ((dy - 7) / 9) ** 2 < 1 or (dy > 12 and abs(dx - 4) < 16):
            base, sh = c["bib"], c["bibs"]
            hi = False
    elif coat == "siamese":  # dark points: ears and a soft mask over the eyes
        for ex, ey in (EAR_L, EAR_R):
            if math.hypot(x - ex, y - ey) < 9:
                base, sh = c["point"], c["points"]
        if ((dx - 1) / 13) ** 2 + ((dy - 6) / 9) ** 2 < 1:  # dark muzzle
            base, sh = c["point"], c["points"]
    col = sh if shade else (c["hi"] if hi else base)
    return C(col)


def paint_fur_layer(name, coat, inner_ears=False, paws=False):
    grid = thin_outline(G[name])
    c = COAT[coat]
    im = Image.new("RGBA", (N, N))
    p = im.load()
    for y in range(N):
        for x in range(N):
            k = grid[y][x]
            if k == "line":
                p[x, y] = C(c["line"]) + (255,)
            elif k == "pink":
                p[x, y] = PINK + (255,)
            elif k == "fur":
                if paws:
                    pc = c.get("bib") or ("FFFFFF" if coat == "calico" else (c["point"] if coat == "siamese" else c["base"]))
                    shade = any(at(grid, x - k2, y + k2) in ("line", "") for k2 in (1, 2))
                    p[x, y] = C(c.get("bibs", c["shade"]) if shade and coat != "siamese" else pc) + (255,)
                    if coat == "siamese" and shade:
                        p[x, y] = C(c["points"]) + (255,)
                else:
                    p[x, y] = fur_color(coat, x, y, grid) + (255,)
            elif k == "grey":
                p[x, y] = RAW[name].getpixel((x, y))
    if name.startswith("body"):
        extend_behind_desk(p, c)
    if inner_ears:
        twitch = name == "body_twitch"
        for tip in ((EAR_L, EAR_R) if not twitch else (EAR_L,)):
            tx, ty = tip
            for y in range(ty + 3, ty + 10):
                half = (y - ty) * 0.5 - 1
                for x in range(int(tx - half), int(tx + half) + 1):
                    if 0 <= x < N and grid[y][x] == "fur" and all(at(grid, x + dx, y + dy) == "fur" for dx, dy in ((1, 0), (-1, 0), (0, -1))):
                        p[x, y] = C(c["ear"]) + (255,)
    return im


def table_y(x):
    a, b = TABLE
    return int(round(a * x + b))


def extend_behind_desk(p, c):
    """Where the body's bottom edge sits just above the (straight) table edge, carry the fur
    down behind the desk. Otherwise the 2 px breathing bob shows sky under the cat."""
    shade = C(c["shade"]) + (255,)
    line = C(c["line"]) + (255,)
    filled = set()
    for x in range(N):
        ys = [y for y in range(N) if p[x, y][3]]
        if not ys:
            continue
        ylow, target = max(ys), table_y(x) + 4
        if not (0 < target - ylow <= 12):
            continue
        y = ylow  # drop the bottom outline run: the cat continues behind the desk
        while y > 0 and p[x, y][:3] == line[:3]:
            p[x, y] = shade
            filled.add((x, y))
            y -= 1
        for y in range(ylow + 1, min(N, target + 1)):
            p[x, y] = shade
            filled.add((x, y))
    for (x, y) in filled:  # keep the side outline on the left/right ends
        if (x - 1 >= 0 and p[x - 1, y][3] == 0) or (x + 1 < N and p[x + 1, y][3] == 0):
            p[x, y] = line


def paint_face(name, coat, look=0):
    """eyes with iris + shine, mouth, blush. look = -1/+1 shifts the eyes."""
    grid = G[name]
    c = COAT[coat]
    iris, glint = EYES[c["eyes"]]
    mouth_col = "D7D7E2" if coat in ("black", "siamese") else c["line"]  # light mouth on dark fur
    im = Image.new("RGBA", (N, N))
    p = im.load()
    eyes = [b for b in blobs(grid, "line") if len(b) >= 30]
    eye_px = set(q for b in eyes for q in b)
    for y in range(N):
        for x in range(N):
            if grid[y][x] == "line" and (x, y) not in eye_px:
                p[x, y] = C(mouth_col) + (255,)
            elif grid[y][x] == "pink":
                p[x, y] = PINK + (255,)
    for b in eyes:
        xs = [q[0] for q in b]; ys = [q[1] for q in b]
        x0, y0 = min(xs), min(ys)
        for (x, y) in b:
            nx = x + look
            p[nx, y] = C(iris) + (255,)
            if c["eyes"] != "dark":  # cat-like slit pupil in coloured eyes, clear of the edge
                mid = (x0 + max(xs)) / 2
                if abs(x + 0.5 - mid - 0.5) < 1 and y0 + 2 <= y <= max(ys) - 1:
                    p[nx, y] = C("101016") + (255,)
        p[x0 + 1 + look, y0 + 1] = C(glint) + (255,)  # shine
    # blush under the outer side of each eye
    if name in ("face_stock", "face_happy", "face_blink", "face_sleepy"):
        for (ex, ey), side in ((EYE_L, -1), (EYE_R, 1)):
            bx, by = int(ex + side * 4), int(ey + 5)
            for dx in range(-2, 2):
                for dy in range(0, 2):
                    if p[bx + dx, by + dy][3] == 0:
                        p[bx + dx, by + dy] = BLUSH + (200,)
    return im


def mouth_box():
    """bounding box of the ω mouth in the stock face (the non-eye outline blobs)"""
    pts = [q for b in blobs(G["face_stock"], "line") if len(b) < 30 for q in b]
    xs = [q[0] for q in pts]; ys = [q[1] for q in pts]
    return min(xs), min(ys), max(xs), max(ys)


MOUTH = mouth_box()


def paint_yawn(coat, wide):
    """closed eyes + the ω mouth replaced by an open mouth at the same spot.
    wide=False: opening/closing frame, True: full yawn."""
    im = paint_face("face_sleepy", coat)
    p = im.load()
    x0, y0, x1, y1 = MOUTH
    for y in range(y0 - 2, y1 + 3):  # remove every bit of the old mouth
        for x in range(x0 - 2, x1 + 3):
            if p[x, y][3] and abs(x - EYE_L[0]) > 4 and abs(x - EYE_R[0]) > 4:
                p[x, y] = (0, 0, 0, 0)
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2 + 1
    rx, ry = (3, 2) if not wide else (4, 5)
    d = ImageDraw.Draw(im)
    line = C("D7D7E2" if coat in ("black", "siamese") else COAT[coat]["line"]) + (255,)
    d.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=C("7A2B3A") + (255,), outline=line)
    if wide:  # tongue
        d.line([(cx - 2, cy + ry - 2), (cx + 2, cy + ry - 2)], fill=PINK + (255,))
        d.point([(cx - 1, cy + ry - 3), (cx, cy + ry - 3), (cx + 1, cy + ry - 3)], fill=PINK + (255,))
    return im


# ------------------------------------------------------------------ desk + keyboard
def desk():
    """Wooden desk below a straight table edge, and the original keyboard.

    The edge is a least-squares line through the original table line (the Scale2x'd
    staircase looked jagged). The keyboard is the original's pixels at exactly 2x, so it
    keeps the original's angle, just recoloured."""
    src = load_8bit(EIGHT["desk"])
    g64 = classify(src)
    n64 = 64

    def at64(x, y):
        return g64[y][x] if 0 <= x < n64 and 0 <= y < n64 else ""
    # keyboard = grey keys + the outline pixels touching them; the rest of the outline is the table edge
    kb = set()
    for y in range(n64):
        for x in range(n64):
            k = g64[y][x]
            if k == "grey" or (k == "line" and any(at64(x + dx, y + dy) == "grey" for dx in (-1, 0, 1) for dy in (-1, 0, 1))):
                kb.add((x, y))
    # close the keyboard's outer outline: outline pixels next to keyboard outline pixels
    for _ in range(2):
        add = set()
        for y in range(n64):
            for x in range(n64):
                if g64[y][x] == "line" and (x, y) not in kb:
                    nb = sum((x + dx, y + dy) in kb for dx in (-1, 0, 1) for dy in (-1, 0, 1))
                    if nb >= 3:
                        add.add((x, y))
        kb |= add
    edge = [(x, y) for y in range(n64) for x in range(n64) if g64[y][x] == "line" and (x, y) not in kb]
    # straight line through the edge (in HD coordinates)
    xs = [2 * x + 1 for x, _ in edge]
    ys = [2 * y + 1 for _, y in edge]
    n = len(xs)
    mx, my = sum(xs) / n, sum(ys) / n
    a, b = TABLE

    im = Image.new("RGBA", (N, N))
    p = im.load()
    wood, wood_d, wood_l, edge_c = C("C88A5A"), C("A86E43"), C("DCA273"), C("3A2210")
    for x in range(N):
        y0 = int(round(a * x + b))
        for y in range(max(0, y0), N):
            col = wood_l if y - y0 in (1, 2) else wood
            if y - y0 > 4 and (x * 3 + y * 7) % 23 == 0:
                col = wood_d
            p[x, y] = col + (255,)
        if 0 <= y0 < N:
            p[x, y0] = edge_c + (255,)
    # keyboard: original pixels, 2x, recoloured
    for (x, y) in kb:
        r, g, bl, al = src.getpixel((x, y))
        k = g64[y][x]
        if k == "line":
            inner = all((x + dx, y + dy) in kb for dx in (-1, 0, 1) for dy in (-1, 0, 1))
            col = C("4A4E5E") if inner else C("15151C")  # slate grout between keys, black rim
        else:
            col = C("CDD0DA") if (r + g + bl) / 3 > 130 else C("8E92A3")
        for dx in (0, 1):
            for dy in (0, 1):
                p[2 * x + dx, 2 * y + dy] = col + (255,)
    return im


# ------------------------------------------------------------------ effects
def hearts(frame):
    im = Image.new("RGBA", (N, N))
    shape = ["01100110", "11111111", "11111111", "01111110", "00111100", "00011000"]
    for i, (hx, hy) in enumerate(((FACE_C[0] + 26, FACE_C[1] - 22), (FACE_C[0] + 38, FACE_C[1] - 30))):
        oy = -frame * 3 - i * 2
        for y, row in enumerate(shape):
            for x, ch in enumerate(row):
                if ch == "1":
                    col = C("FF5C7A") if not (x == 1 and y == 1) else C("FFC2CF")
                    im.putpixel((int(hx + x), int(hy + y + oy)), col + (255,))
    return im


def fx(name):
    """impact lines: Scale2x of the originals"""
    return RAW[name].copy()


ZS = [["####", "..#.", ".#..", "####"],
      ["#####", "...#.", "..#..", ".#...", "#####"],
      ["######", "....#.", "...#..", "..#...", ".#....", "######"]]


def zzz(frame):
    """three small z's drifting up from behind the head: white with an ink outline so
    they read on any sky or coat"""
    im = Image.new("RGBA", (N, N))
    p = im.load()
    # rise from between the ears, clear of the sun/moon (which sit behind the right ear)
    bx, by = EAR_L[0] + 8, EAR_L[1] + 2
    letters = set()
    for j, z in enumerate(ZS):
        ox, oy = bx + j * 7, by - j * 6 - frame * 2
        for y, row in enumerate(z):
            for x, ch in enumerate(row):
                if ch == "#":
                    letters.add((ox + x, oy + y))
    for (x, y) in letters:
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                q = (x + dx, y + dy)
                if q not in letters:
                    p[q] = C("202838") + (255,)
    for q in letters:
        p[q] = C("F8F8F0") + (255,)
    return im


# ------------------------------------------------------------------ all layers
LAYERS = ["body", "body_twitch", "face_stock", "face_happy", "face_blink", "face_sleepy", "face_look_l",
          "face_look_r", "face_yawn", "face_yawn_half", "desk", "paws_up", "paws_left", "paws_right", "fx_left", "fx_right",
          "fx_zzz0", "fx_zzz1", "fx_zzz2", "fx_heart0", "fx_heart1",
          "gear2", "gear3", "gear4", "fluff", "desk_rgb", "stand1", "stand2", "stand3", "stand4"]
STAND_ZOOM = [0.62, 0.70, 0.78, 0.86]  # the evolution scene's cat grows a size each stage


def hd_layers():
    out = {}
    d = desk()
    for coat in COATS:
        out[f"hd/{coat}/body"] = paint_fur_layer("body", coat, inner_ears=True)
        out[f"hd/{coat}/body_twitch"] = paint_fur_layer("body_twitch", coat, inner_ears=True)
        for f in ("stock", "happy", "blink", "sleepy"):
            out[f"hd/{coat}/face_{f}"] = paint_face(f"face_{f}", coat)
        out[f"hd/{coat}/face_look_l"] = paint_face("face_stock", coat, look=-1)
        out[f"hd/{coat}/face_look_r"] = paint_face("face_stock", coat, look=1)
        out[f"hd/{coat}/face_yawn"] = paint_yawn(coat, wide=True)
        out[f"hd/{coat}/face_yawn_half"] = paint_yawn(coat, wide=False)
        for w in ("up", "left", "right"):
            out[f"hd/{coat}/paws_{w}"] = paint_fur_layer(f"paws_{w}", coat, paws=True)
        out[f"hd/{coat}/fluff"] = evolution.fluff(coat, sys.modules[__name__])
        for st in range(1, 5):  # full-body cat for the evolution scene, one per stage
            out[f"hd/{coat}/stand{st}"] = heropose.build("stand", coat, sys.modules[__name__], stage=st, zoom=STAND_ZOOM[st - 1])
    out["hd/-/desk"] = d
    out["hd/-/desk_rgb"] = evolution.desk_rgb(d)
    for st in (2, 3, 4):
        out[f"hd/-/gear{st}"] = evolution.gear(st, sys.modules[__name__])
    for n in ("fx_left", "fx_right"):
        out[f"hd/-/{n}"] = fx(n)
    for f in range(3):
        out[f"hd/-/fx_zzz{f}"] = zzz(f)
    out["hd/-/fx_heart0"] = hearts(0)
    out["hd/-/fx_heart1"] = hearts(1)
    return out


def get(layers, coat, name):
    return layers.get(f"hd/{coat}/{name}") or layers.get(f"hd/-/{name}")


def compose(layers, coat, face="stock", paws="up", fx=None, twitch=False, bg=(176, 204, 232), size=4):
    im = Image.new("RGBA", (N, N), bg + (255,))
    for n in ["body_twitch" if twitch else "body", f"face_{face}", "desk", f"paws_{paws}" if paws else None, fx]:
        if n:
            l = get(layers, coat, n)
            if l:
                im.alpha_composite(l)
    return im.resize((N * size, N * size), Image.NEAREST)


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    L = hd_layers()
    sheet = Image.new("RGBA", (4 * 266, 2 * 266), "white")
    for i, coat in enumerate(COATS):
        sheet.alpha_composite(compose(L, coat, size=2), ((i % 4) * 266, (i // 4) * 266))
    sheet.save(os.path.join(OUT, "hd_coats.png"))
    poses = Image.new("RGBA", (4 * 522, 512), "white")
    for i, kw in enumerate([dict(face="stock"), dict(face="happy", paws="left", fx="fx_left"),
                            dict(face="look_r", twitch=True), dict(face="yawn", paws=None)]):
        poses.alpha_composite(compose(L, "orange", **kw), (i * 522, 0))
    poses.save(os.path.join(OUT, "hd_poses.png"))
    print("eyes", EYE_L, EYE_R, "ears", EAR_L, EAR_R)
    if "--export" in sys.argv:
        import export_hd
        export_hd.export(L, compose)
