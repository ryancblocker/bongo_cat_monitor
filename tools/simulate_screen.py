#!/usr/bin/env python3
"""Draw the Bongo Cat screen to a PNG without the board.

Uses the real sprite data (animations/*.c) and LVGL's real unscii_16 font
(.pio/libdeps/cyd/lvgl/..., present after one `pio run`), with the same
positions as bongo_cat.ino. It's for checking that text fits before flashing.

    python3 tools/simulate_screen.py out.png
"""
import re, subprocess, sys, os

W, H = 240, 320
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = os.path.join(ROOT, ".pio/libdeps/cyd/lvgl/src/font/lv_font_unscii_16.c")

def load_sprite(rel):
    txt = open(os.path.join(ROOT, "animations", rel)).read()
    body = txt.split("_map[] = {", 1)[1].split("};", 1)[0]
    b = [int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", body)]
    px = []
    for i in range(64 * 64):
        c = b[2 * i] | (b[2 * i + 1] << 8)
        r, g, bl = (c >> 11) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31
        px.append((r, g, bl, b[64 * 64 * 2 + i]))
    return px

def load_font():
    txt = open(FONT).read()
    bm = txt.split("glyph_bitmap[] = {", 1)[1].split("};", 1)[0]
    bm = re.sub(r"/\*.*?\*/", "", bm, flags=re.S)
    bitmap = [int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]+)", bm)]
    dsc = re.findall(r"\.bitmap_index = (\d+), \.adv_w = (\d+), \.box_w = (\d+), \.box_h = (\d+), \.ofs_x = (-?\d+), \.ofs_y = (-?\d+)", txt)
    glyphs = {}
    for i, d in enumerate(dsc[1:]):          # id 0 is reserved; id 1 = U+0020
        glyphs[chr(32 + i)] = tuple(int(v) for v in d)
    lh = int(re.search(r"\.line_height = (\d+)", txt).group(1))
    return bitmap, glyphs, lh

screen = [[(255, 255, 255)] * W for _ in range(H)]
bitmap, glyphs, LINE_H = load_font()

def text_width(s):
    return sum(glyphs[c][1] // 16 for c in s)

def draw_text(s, x, y, color=(0, 0, 0)):
    for ch in s:
        bi, adv, bw, bh, ox, oy = glyphs.get(ch, glyphs["?"])
        top = y + LINE_H - bh - oy
        bit = bi * 8
        for gy in range(bh):
            for gx in range(bw):
                if bitmap[bit >> 3] & (0x80 >> (bit & 7)):
                    X, Y = x + ox + gx, top + gy
                    if 0 <= X < W and 0 <= Y < H:
                        screen[Y][X] = color
                bit += 1
        x += adv // 16

def row_y(n):
    return 4 + n * 20

def place(s, row, right=False):
    if not s:
        return
    x = W - 5 - text_width(s) if right else 5
    draw_text(s, x, row_y(row))

def draw_cat(layers):
    for rel in layers:
        for i, (r, g, b, a) in enumerate(load_sprite(rel)):
            if not a:
                continue
            sx, sy = i % 64, i // 64
            for dy in range(4):
                for dx in range(4):
                    X, Y = 4 + sx * 4 + dx, 82 + sy * 4 + dy
                    if 0 <= X < W and 0 <= Y < H:
                        o = screen[Y][X]
                        screen[Y][X] = tuple((c * a + oc * (255 - a)) // 255 for c, oc in zip((r, g, b), o))

def render(state, out):
    global screen
    screen = [[(255, 255, 255)] * W for _ in range(H)]
    if state.get("sleep"):
        draw_cat(["body/standardbody1.c", "faces/sleepy_face.c", "table/table1.c", "effects/sleepy3.c"])
    else:
        draw_cat(["body/standardbody1.c", "faces/happy_face.c", "table/table1.c", "paws/leftpawdown.c", "effects/left_click_effect.c"])
    place(state["time"], 0); place(state["temp"], 0, right=True)
    place(state["date"], 1); place(state["cond"], 1, right=True)
    rows = [r for r in state["stats"] if r]
    for i, r in enumerate(rows):
        place(r, 2 + i)
    ppm = out + ".ppm"
    with open(ppm, "wb") as f:
        f.write(b"P6 %d %d 255\n" % (W, H))
        f.write(bytes(v for row in screen for px in row for v in px))
    subprocess.run(["sips", "-s", "format", "png", "-z", str(H * 2), str(W * 2), ppm, "--out", out], check=True, capture_output=True)
    os.remove(ppm)

if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "screen.png"
    base = out[:-4] if out.endswith(".png") else out
    # Worst-case widths: the longest strings each field can produce
    render({"time": "12:59PM", "temp": "100F", "date": "WED 30", "cond": "DRIZZLE",
            "stats": ["CPU: 100% 100C", "GPU: 100% 100C", "RAM: 100%", "WPM: 188"]}, base + "_typing.png")
    render({"sleep": True, "time": "9:41PM", "temp": "54F", "date": "THU 12", "cond": "CLOUDY",
            "stats": ["CPU: 4% 41C", "", "RAM: 52%", "WPM: 0"]}, base + "_sleep_nogpu.png")
    print("wrote", base + "_typing.png", base + "_sleep_nogpu.png")
