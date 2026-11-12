"""Full-body chubby cat for the level-up scene: a round bongo-style head with pointed ears on
a big round belly, mitt paws, little feet and a curled tail. Same style as the desk cat:
1 px outline, shading from the top right, the ω face, pink beans, and the 8 coats.

    fly  : powered up mid-air, one fist raised beside the head, the other paw on the chest
    land : superhero landing, squashed down, one paw planted on the ground, one fist up, wink

Parts are drawn back to front, each with its own outline, so an arm in front of the belly
gets a line where it overlaps.
"""
from PIL import Image, ImageDraw, ImageFilter

N = 128
FAT = 6        # extra belly width each side (0 = the slimmer version)
HEAD_RX = 29   # head half-width
GROUND = 118  # HD row the landing pose stands on


def _parts(pose):
    """[(name, draw(d), paws)] back to front, plus the face centre and wink flag.
    paws = [(x, y, r, beans)]: round ends of the arms (and feet) for coat colours / pink beans."""
    def ell(box):
        return lambda d: d.ellipse(box, fill=255)

    def limb(pts, w):
        # rounded ends (a plain wide line ends flat)
        def f(d):
            d.line(pts, fill=255, width=w, joint="curve")
            r = w / 2
            for (x, y) in (pts[0], pts[-1]):
                d.ellipse([x - r, y - r, x + r, y + r], fill=255)
        return f

    def head(cx, cy):
        def f(d):
            d.ellipse([cx - HEAD_RX, cy - 19, cx + HEAD_RX, cy + 19], fill=255)
            d.polygon([(cx - 24, cy - 8), (cx - 19, cy - 31), (cx - 5, cy - 17)], fill=255)
            d.polygon([(cx + 5, cy - 17), (cx + 19, cy - 31), (cx + 24, cy - 8)], fill=255)
        return f

    ARM_W = 12  # arms are short stubs, like the original's paws, with a rounded end the same width

    def body_with_arms(box, arms, hd):
        """head, belly and arms as ONE silhouette: no chin line and no line where an arm starts,
        just the outline around the outside"""
        def f(d):
            hd(d)
            d.ellipse(box, fill=255)
            # no neck: straight sides from the cheeks down to the widest part of the belly
            midy = (box[1] + box[3]) // 2
            d.polygon([(hx - HEAD_RX, hy), (hx + HEAD_RX, hy), (box[2], midy), (box[0], midy)], fill=255)
            for (a, b) in arms:
                d.line([a, b], fill=255, width=ARM_W)
                r = ARM_W // 2
                d.ellipse([b[0] - r, b[1] - r, b[0] + r, b[1] + r], fill=255)  # rounded paw end
        return f

    def foot(box):
        return [((box[0] + box[2]) / 2, (box[1] + box[3]) / 2, max(box[2] - box[0], box[3] - box[1]) / 2 + 1, False)]

    if pose == "land":
        hx, hy = 60, 52
        up, down = ((92 + FAT, 74), (102 + FAT, 54)), ((42, 96), (34, 112))  # planted paw: on the ground, in front of the left foot
        fb, fk = [88, 107, 102, 119], [26, 107, 40, 119]  # feet about as wide as the arms
        return [
            ("tail", limb([(34, 100), (18, 98), (10, 86), (11, 75), (17, 69), (23, 71)], 8), []),
            ("foot_back", ell(fb), foot(fb)),
            ("foot_knee", ell(fk), foot(fk)),  # behind the body, so the planted paw sits in front of it
            ("body", body_with_arms([30 - FAT, 59, 98 + FAT, 117], [up, down], head(hx, hy)),
             # no beans: in the landing both paws face away from us (fist up, paw pressed to the ground)
             [(up[1][0], up[1][1], ARM_W / 2 + 1, False), (down[1][0], down[1][1], ARM_W / 2 + 1, False)]),
        ], (hx, hy), True
    hx, hy = 62, 40  # fly
    up, side = ((90 + FAT, 62), (102 + FAT, 40)), ((38 - FAT, 72), (26 - FAT, 84))
    fl, fr = [46, 105, 60, 118], [70, 105, 84, 118]  # feet about as wide as the arms
    return [
        ("tail", limb([(88, 96), (106, 98), (116, 87), (116, 77), (111, 71), (105, 73)], 8), []),  # right side, clear of the side arm
        ("foot_l", ell(fl), foot(fl)),
        ("foot_r", ell(fr), foot(fr)),
        ("body", body_with_arms([32 - FAT, 45, 96 + FAT, 109], [up, side], head(hx, hy)),
         [(up[1][0], up[1][1], ARM_W / 2 + 1, True), (side[1][0], side[1][1], ARM_W / 2 + 1, False)]),
    ], (hx, hy), False


ZOOM = 0.8              # level-up camera: the cat is drawn at 80% (feet stay on the ground line)
ANCHOR = (64, 119)      # scale around the middle of the ground line


def _shrink(m):
    """scale a shape mask by ZOOM around ANCHOR (before outlining, so the outline stays 1 px)"""
    if ZOOM == 1:
        return m
    ns = round(N * ZOOM)
    small = m.resize((ns, ns), Image.BILINEAR).point(lambda v: 255 if v >= 128 else 0)
    out = Image.new("L", (N, N), 0)
    out.paste(small, (round(ANCHOR[0] - ANCHOR[0] * ZOOM), round(ANCHOR[1] - ANCHOR[1] * ZOOM)))
    return out


def _new(x, y):
    return ANCHOR[0] + (x - ANCHOR[0]) * ZOOM, ANCHOR[1] + (y - ANCHOR[1]) * ZOOM


def _old(x, y):
    return ANCHOR[0] + (x - ANCHOR[0]) / ZOOM, ANCHOR[1] + (y - ANCHOR[1]) / ZOOM


def build(pose, coat, pc):
    """RGBA image of the pose in the given coat. pc = the pixelcat module (colours).
    Shapes are designed on the 128 grid and drawn at ZOOM; colour patterns are worked out
    in the design coordinates so they shrink with the cat."""
    parts, (fx, fy), wink = _parts(pose)
    c = pc.COAT[coat]
    kind = [[""] * N for _ in range(N)]
    part = [[""] * N for _ in range(N)]
    paw_px = set()  # pixels of paws (the round ends of the arms) and feet
    for name, draw, paws in parts:
        m = Image.new("L", (N, N), 0)
        draw(ImageDraw.Draw(m))
        m = _shrink(m)
        ol = m.filter(ImageFilter.MaxFilter(3))
        mp, op = m.load(), ol.load()
        for y in range(N):
            for x in range(N):
                if mp[x, y]:
                    kind[y][x], part[y][x] = "fur", name
                    xo, yo = _old(x, y)
                    if any((xo - px) ** 2 + (yo - py) ** 2 <= r * r for px, py, r, _ in paws):
                        paw_px.add((x, y))
                    else:
                        paw_px.discard((x, y))
                elif op[x, y]:
                    kind[y][x], part[y][x] = "line", name

    # the head is part of the body's silhouette; tag its area so face colours still apply
    hm = Image.new("L", (N, N), 0)
    hd = ImageDraw.Draw(hm)
    hd.ellipse([fx - HEAD_RX, fy - 19, fx + HEAD_RX, fy + 19], fill=255)
    hd.polygon([(fx - 24, fy - 8), (fx - 19, fy - 31), (fx - 5, fy - 17)], fill=255)
    hd.polygon([(fx + 5, fy - 17), (fx + 19, fy - 31), (fx + 24, fy - 8)], fill=255)
    hmp = _shrink(hm).load()
    for y in range(N):
        for x in range(N):
            if hmp[x, y] and part[y][x] == "body" and kind[y][x] == "fur" and (x, y) not in paw_px:
                part[y][x] = "head"

    at = lambda x, y: kind[y][x] if 0 <= x < N and 0 <= y < N else ""
    ears = [(fx - 19, fy - 31), (fx + 19, fy - 31)]  # design coordinates

    def colour(x, y):
        nm = part[y][x]
        shade = any(at(x - k, y + k) in ("line", "") for k in (1, 2, 3))
        hi = coat != "white" and at(x + 1, y - 1) in ("line", "")
        base, sh = c["base"], c["shade"]
        xo, yo = _old(x, y)
        dx, dy = xo - fx, yo - fy
        muzzle = nm == "head" and (dx / 13) ** 2 + ((dy - 6) / 8) ** 2 < 1
        is_paw = (x, y) in paw_px
        if coat == "orange":
            stripe = False
            if nm == "head":
                # forehead "M": three short marks, the middle one longer
                stripe = ((5.5 <= abs(dx) <= 7.5) and -16 < dy < -10) or (abs(dx + 0.5) <= 1.2 and -17 < dy < -8)
                # two short cheek stripes on each side
                if HEAD_RX - 10 < abs(dx) < HEAD_RX - 1 and (2.5 <= dy <= 4.5 or 7.5 <= dy <= 9.5):
                    stripe = True
            elif nm == "body" and not is_paw:
                # bands that wrap around the round belly, thick on the sides, thin toward the middle
                side = abs(dx)
                s_ = (yo + 0.018 * dx * dx) % 11
                width = 3.5 if side > 26 else (2.5 if side > 19 else (1.5 if side > 14 else 0))
                stripe = yo > fy + 16 and s_ < width
            elif nm == "tail":  # rings
                stripe = (xo + yo) % 9 < 3.5
            if stripe:
                base = sh = c["stripe"]
        elif coat == "calico":
            if nm == "head" and (xo - ears[0][0]) ** 2 / 90 + (yo - ears[0][1] - 16) ** 2 / 50 < 1:
                base, sh = c["patchA"], c["patchAs"]
            elif nm == "head" and (xo - ears[1][0]) ** 2 / 70 + (yo - ears[1][1] - 16) ** 2 / 50 < 1:
                base, sh = c["patchB"], c["patchBs"]
            elif nm == "body" and ((xo - fx - 24) / 16) ** 2 + ((yo - fy - 36) / 15) ** 2 < 1:  # round patch on the side
                base, sh = c["patchA"], c["patchAs"]
            elif nm == "tail":
                base, sh = c["patchB"], c["patchBs"]
        elif coat == "tuxedo":
            belly = nm == "body" and ((xo - fx - 2) / 17) ** 2 + ((yo - (fy + 44)) / 22) ** 2 < 1
            if muzzle or is_paw or belly:
                base, sh = c["bib"], c["bibs"]
                hi = False
        elif coat == "siamese":
            near_ear = any(((xo - ex) ** 2 + (yo - ey) ** 2) ** 0.5 < 10 for ex, ey in ears)
            if is_paw or nm == "tail" or near_ear or (nm == "head" and (dx / 14) ** 2 + ((dy - 5) / 9) ** 2 < 1):
                base, sh = c["point"], c["points"]
        if is_paw and coat == "calico":
            base, sh = "FFFFFF", c["shade"]
        return pc.C(sh if shade else (c["hi"] if hi else base))

    im = Image.new("RGBA", (N, N))
    p = im.load()
    for y in range(N):
        for x in range(N):
            if kind[y][x] == "line":
                p[x, y] = pc.C(c["line"]) + (255,)
            elif kind[y][x] == "fur":
                p[x, y] = colour(x, y) + (255,)
    # inner ears (worked out in design coordinates)
    for y in range(N):
        for x in range(N):
            if part[y][x] != "head" or kind[y][x] != "fur":
                continue
            if not all(at(x + a, y + b) == "fur" for a, b in ((1, 0), (-1, 0), (0, -1))):
                continue
            xo, yo = _old(x, y)
            for ex, ey in ears:
                if 5 <= yo - ey < 13 and abs(xo - ex) <= (yo - ey) * 0.42 - 1:
                    p[x, y] = pc.C(c["ear"]) + (255,)
    # pink beans on the paws that face us
    for _, _, paws in parts:
        for (px, py, _, beans) in paws:
            if not beans:
                continue
            nx, ny = _new(px, py)
            mx, my = int(round(nx)), int(round(ny))
            for (bx, by, w, h) in ((mx - 1, my, 3, 2), (mx - 3, my - 2, 2, 1), (mx, my - 3, 1, 2), (mx + 2, my - 2, 2, 1)):
                for yy in range(by, by + h):
                    for xx in range(bx, bx + w):
                        if 0 <= xx < N and 0 <= yy < N and kind[yy][xx] == "fur" and (xx, yy) in paw_px:
                            p[xx, yy] = pc.PINK + (255,)

    # face, in the desk cat's style: blob eyes with a shine, ω mouth, blush
    iris, glint = pc.EYES[c["eyes"]]
    line = pc.C("D7D7E2" if coat in ("black",) else c["line"]) + (255,)
    d = ImageDraw.Draw(im)
    for side in (-1, 1):
        ex, ey = (int(round(v)) for v in _new(fx + side * 11, fy + 1))
        if wink and side == -1:  # closed, happy "^" eye
            d.line([(ex - 3, ey + 1), (ex, ey - 2), (ex + 3, ey + 1)], fill=line)
            d.line([(ex - 3, ey + 2), (ex, ey - 1), (ex + 3, ey + 2)], fill=line)
        else:
            d.ellipse([ex - 3, ey - 3, ex + 2, ey + 3], fill=pc.C(iris) + (255,))
            if c["eyes"] != "dark":
                d.line([(ex, ey - 1), (ex, ey + 1)], fill=pc.C("101016") + (255,))
            p[ex - 2, ey - 2] = pc.C(glint) + (255,)
        for bx in range(-2, 2):
            for by in range(0, 2):
                q = (ex + side * 4 + bx, ey + 5 + by)
                if at(*q) == "fur":
                    p[q] = pc.BLUSH + (255,)
    mx, my = (int(round(v)) for v in _new(fx, fy + 8))  # ω mouth
    d.line([(mx - 4, my), (mx - 3, my + 2), (mx - 1, my + 2), (mx, my), (mx + 1, my + 2), (mx + 3, my + 2), (mx + 4, my)], fill=line)
    d.point([(mx - 1, my - 3), (mx, my - 3), (mx + 1, my - 3), (mx, my - 2)], fill=pc.PINK + (255,))  # nose
    return im
