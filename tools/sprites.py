"""Shared sprite helpers: load the original 64x64 sprites (animations/*.c) and pack
images into the firmware's palette + run-length format."""
import os, re
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


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


def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def pack(img, scale):
    """-> dict(x, y, w, h, scale, pal565, alpha, rows, data) or None if empty."""
    img = img.convert("RGBA")
    # clear colour of invisible pixels so they quantise together
    px = img.load()
    for y in range(img.height):
        for x in range(img.width):
            if px[x, y][3] < 6:
                px[x, y] = (0, 0, 0, 0)
    bbox = img.getchannel("A").getbbox()
    if not bbox:
        return None
    img = img.crop(bbox)
    q = img.quantize(colors=255, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.NONE)
    pal = q.getpalette(rawmode="RGBA")
    n = len(pal) // 4
    # remap: every (near-)transparent palette entry -> 0, the rest -> 1..
    remap, out_pal = {}, [(0, 0, 0, 0)]
    for i in range(n):
        r, g, b, a = pal[4 * i: 4 * i + 4]
        if a < 6:
            remap[i] = 0
        else:
            remap[i] = len(out_pal)
            out_pal.append((r, g, b, a))
    assert len(out_pal) <= 256, len(out_pal)
    w, h = q.size
    qd = q.load()
    data, rows = bytearray(), []
    for y in range(h):
        rows.append(len(data))
        x = 0
        while x < w:
            idx = remap[qd[x, y]]
            run = 1
            while x + run < w and run < 255 and remap[qd[x + run, y]] == idx:
                run += 1
            data += bytes((run, idx))
            x += run
    return dict(x=bbox[0], y=bbox[1], w=w, h=h, scale=scale,
                pal565=[rgb565(*c[:3]) for c in out_pal], alpha=[c[3] for c in out_pal], rows=rows, data=data)
