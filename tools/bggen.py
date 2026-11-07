# Writes the parallax backdrops as .pix files into assets/sprites.
#
#   python tools/bggen.py
#
# They're silhouettes built from periodic noise, so every layer tiles side by
# side without a seam. Tweak the numbers here and rerun rather than editing
# the .pix output by hand, it gets overwritten.

import math
import os
import random

OUT = os.path.join(os.path.dirname(__file__), "..", "assets", "sprites")


def ridge(w, parts, seed):
    """Height profile that wraps around at w. parts: (waves, amplitude)."""
    rnd = random.Random(seed)
    phases = [(k, a, rnd.uniform(0, 6.283)) for k, a in parts]
    return [sum(a * math.sin(6.283 * k * x / w + p) for k, a, p in phases) for x in range(w)]


def spikes(w, count, width, height, seed, jitter=0.4, power=1.0):
    """Pointy bumps (stalagmites or stalactites) spread around the width.
    power above 1 makes needles, below 1 blunt columns."""
    rnd = random.Random(seed)
    out = [0.0] * w
    for i in range(count):
        cx = (i + rnd.uniform(-jitter, jitter)) * w / count
        hw = width * rnd.uniform(0.6, 1.3)
        hh = height * rnd.uniform(0.5, 1.2)
        for x in range(w):
            d = min(abs(x - cx), w - abs(x - cx))     # wrap around
            if d < hw:
                out[x] = max(out[x], hh * (1 - d / hw) ** power)
    return out


def build(w, h, heights, from_top, fill="a", rim="b", speckle=None, seed=0):
    rnd = random.Random(seed)
    rows = [["."] * w for _ in range(h)]
    for x in range(w):
        top = int(round(h - heights[x])) if not from_top else 0
        bottom = h if not from_top else int(round(heights[x]))
        for y in range(max(0, top), min(h, bottom)):
            rows[y][x] = fill
    # a lit edge on the side facing the light (up and to the left)
    for y in range(h):
        for x in range(w):
            if rows[y][x] != fill:
                continue
            above = rows[y - 1][x] if y > 0 else "."
            left = rows[y][x - 1] if x > 0 else rows[y][w - 1]
            if (not from_top and above == ".") or left == ".":
                rows[y][x] = rim
    if speckle:
        ch, chance = speckle
        for y in range(h):
            for x in range(w):
                if rows[y][x] == fill and rnd.random() < chance:
                    rows[y][x] = ch
    return ["".join(r) for r in rows]


def write(name, w, h, rows, palette, comment):
    path = os.path.join(OUT, name + ".pix")
    with open(path, "w", newline="\n") as f:
        f.write("// %s\n// made by tools/bggen.py, change it there\n" % comment)
        f.write("size %d %d\n" % (w, h))
        f.write("palette\n")
        for ch, col in palette:
            f.write("%s %s\n" % (ch, col))
        f.write("\nframe 0\n")
        f.write("\n".join(rows) + "\n")
    print("wrote", path)


# far away: low rolling ridge with a few thin spires, barely lighter than the sky
w, h = 192, 72
hts = [a + b + 26 for a, b in zip(ridge(w, [(1, 8), (3, 5), (7, 2)], 1), spikes(w, 5, 8, 38, 2, power=1.3))]
write("bg_far", w, h, build(w, h, hts, False, speckle=("c", 0.004), seed=3),
      [("a", "#1a1527"), ("b", "#221c33"), ("c", "#4a4270")], "distant ridge")

# ceiling seen from far, hangs from the top of the screen
w, h = 192, 44
hts = [a + b + 10 for a, b in zip(ridge(w, [(2, 4), (5, 3)], 4), spikes(w, 9, 7, 30, 5, power=1.2))]
write("bg_ceiling", w, h, build(w, h, hts, True, seed=6),
      [("a", "#15111f"), ("b", "#1d182b")], "far stalactites")

# middle: stalagmites, some tall enough to read as columns
w, h = 160, 100
hts = [a + b + 14 for a, b in zip(ridge(w, [(1, 5), (4, 4), (9, 2)], 7), spikes(w, 4, 17, 84, 8, 0.3, power=0.75))]
write("bg_mid", w, h, build(w, h, hts, False, speckle=("g", 0.01), seed=9),
      [("a", "#120e1b"), ("b", "#2a2340"), ("g", "#1d2b20")], "stalagmites")

# near: big dark pillars with a few glowing mushrooms
w, h = 128, 124
hts = [a + b + 8 for a, b in zip(ridge(w, [(2, 4), (6, 2)], 10), spikes(w, 2, 22, 110, 11, 0.2, power=0.45))]
write("bg_near", w, h, build(w, h, hts, False, speckle=("p", 0.006), seed=12),
      [("a", "#0d0a13"), ("b", "#261f38"), ("p", "#6a4a90")], "near pillars")

# in front of everything: roots and rock hanging into the top of the view
w, h = 112, 34
hts = [max(0, b + a) for a, b in zip(ridge(w, [(3, 3)], 13), spikes(w, 3, 10, 32, 14, 0.3, power=1.1))]
write("fg_roots", w, h, build(w, h, hts, True, rim="a", seed=15),
      [("a", "#07050b")], "foreground roots")

# in front, bottom: a lump of rock here and there
w, h = 96, 20
hts = [max(0, b) for b in spikes(w, 2, 20, 18, 16, 0.2, power=0.6)]
write("fg_rocks", w, h, build(w, h, hts, False, rim="a", seed=17),
      [("a", "#07050b")], "foreground rocks")


# ---------------------------------------------------------------------------
# the other worlds' backdrops

def shade(grid, fill="a", rim="b", speckle=None, seed=0, from_top=False):
    """Turn a grid of booleans into rows, with a lit edge like build()."""
    h, w = len(grid), len(grid[0])
    rnd = random.Random(seed)
    rows = [[fill if grid[y][x] else "." for x in range(w)] for y in range(h)]
    for y in range(h):
        for x in range(w):
            if not grid[y][x]:
                continue
            above = grid[y - 1][x] if y > 0 else from_top
            left = grid[y][x - 1] if x > 0 else grid[y][w - 1]
            if not above or not left:
                rows[y][x] = rim
            elif speckle and rnd.random() < speckle[1]:
                rows[y][x] = speckle[0]
    return ["".join(r) for r in rows]


def from_heights(w, h, heights, from_top=False):
    grid = [[False] * w for _ in range(h)]
    for x in range(w):
        v = int(round(heights[x]))
        for y in range(h):
            grid[y][x] = y < v if from_top else y >= h - v
    return grid


def trees(w, h, count, seed, trunk=(2, 4), crown=(8, 14), tall=(0.5, 0.9), jitter=0.35):
    """Round crowns on straight trunks, wrapping around like everything else."""
    rnd = random.Random(seed)
    grid = [[False] * w for _ in range(h)]
    for i in range(count):
        cx = (i + rnd.uniform(-jitter, jitter)) * w / count
        top = h - int(h * rnd.uniform(*tall))
        tw = rnd.randint(*trunk)
        r = rnd.randint(*crown)
        for y in range(max(0, top), h):
            for dx in range(-(tw // 2), tw - tw // 2):
                grid[y][int(cx + dx) % w] = True
        for _ in range(5):
            ox, oy = rnd.uniform(-r * 0.7, r * 0.7), rnd.uniform(-r * 0.4, r * 0.5)
            rr = r * rnd.uniform(0.55, 1.0)
            for y in range(max(0, int(top + oy - rr)), min(h, int(top + oy + rr) + 1)):
                for x in range(int(cx + ox - rr), int(cx + ox + rr) + 1):
                    if (x - cx - ox) ** 2 + (y - top - oy) ** 2 <= rr * rr:
                        grid[y][x % w] = True
    return grid


# forest
w, h = 192, 70
hts = [a + b + 22 for a, b in zip(ridge(w, [(1, 5), (3, 4)], 20), spikes(w, 14, 9, 22, 21, power=0.5))]
write("fo_far", w, h, shade(from_heights(w, h, hts), speckle=("y", 0.002), seed=22),
      [("a", "#16241c"), ("b", "#1f3326"), ("y", "#d8f080")], "far treeline")

w, h = 160, 110
write("fo_mid", w, h, shade(trees(w, h, 5, 23), speckle=("y", 0.0025), seed=24),
      [("a", "#0f1c14"), ("b", "#22382a"), ("y", "#d8f080")], "trees")

w, h = 128, 150
write("fo_near", w, h, shade(trees(w, h, 2, 25, trunk=(8, 12), crown=(20, 28), tall=(0.75, 0.95)), seed=26),
      [("a", "#0a120d"), ("b", "#1b2c20")], "big trees")

w, h = 160, 40
hts = [max(0, a + b + 8) for a, b in zip(ridge(w, [(2, 3), (7, 2)], 27), spikes(w, 11, 8, 26, 28, power=0.6))]
write("fo_canopy", w, h, shade(from_heights(w, h, hts, True), from_top=True, seed=29),
      [("a", "#0c1a10"), ("b", "#18301c")], "leaves overhead")

w, h = 96, 26
hts = [max(0, b) for b in spikes(w, 3, 14, 24, 30, 0.3, power=0.8)]
write("fo_ferns", w, h, shade(from_heights(w, h, hts), rim="a", seed=31),
      [("a", "#050a06")], "ferns in front")

# volcano
w, h = 200, 90
cone = [max(0, 80 - abs(x - 100) * 1.1) for x in range(w)]
hts = [max(c, 14 + r) for c, r in zip(cone, ridge(w, [(2, 4), (5, 3)], 32))]
rows = [list(r) for r in shade(from_heights(w, h, hts), seed=33)]
# a lava run down the side and a glow in the crater
for y in range(h - 80, h - 30):
    x = int(100 + 6 * math.sin(y * 0.25) + (y - (h - 80)) * 0.35)
    for dx in (0, 1):
        if rows[y][x + dx] != ".":
            rows[y][x + dx] = "o" if y % 3 else "y"
for x in range(92, 109):
    if rows[h - 80][x] != ".":
        rows[h - 80][x] = "y"
write("vo_far", w, h, ["".join(r) for r in rows],
      [("a", "#2a1418"), ("b", "#4a2020"), ("o", "#c84020"), ("y", "#ffb040")], "volcano")

w, h = 160, 100
hts = [a + b + 16 for a, b in zip(ridge(w, [(1, 6), (5, 3)], 34), spikes(w, 7, 9, 60, 35, 0.3, power=0.25))]
write("vo_mid", w, h, shade(from_heights(w, h, hts), speckle=("e", 0.004), seed=36),
      [("a", "#1c1014"), ("b", "#3a1c1c"), ("e", "#ff6030")], "basalt columns")

w, h = 128, 130
hts = [a + b + 10 for a, b in zip(ridge(w, [(2, 4)], 37), spikes(w, 2, 18, 110, 38, 0.2, power=0.15))]
write("vo_near", w, h, shade(from_heights(w, h, hts), seed=39),
      [("a", "#120a0c"), ("b", "#301418")], "near basalt")

w, h = 192, 40
hts = [max(0, a + 14) for a in ridge(w, [(2, 6), (5, 4), (11, 2)], 40)]
write("vo_smoke", w, h, shade(from_heights(w, h, hts, True), from_top=True, seed=41),
      [("a", "#1e1216"), ("b", "#2c1a20")], "smoke")

# sea
w, h = 192, 40
hts = [max(0, a + b) for a, b in zip(ridge(w, [(1, 3), (4, 2)], 42), spikes(w, 3, 22, 34, 43, 0.3, power=0.7))]
write("se_islands", w, h, shade(from_heights(w, h, hts), seed=44),
      [("a", "#1c2a44"), ("b", "#2c4064")], "far islands")

w, h = 192, 36
hts = [max(0, a + 12) for a in ridge(w, [(3, 5), (7, 3), (13, 2)], 45)]
write("se_clouds", w, h, shade(from_heights(w, h, hts, True), from_top=True, seed=46),
      [("a", "#1a2440"), ("b", "#2a3656")], "clouds")

w, h = 128, 24
hts = [16 + 3 * math.sin(6.283 * 4 * x / w) + 1.5 * math.sin(6.283 * 9 * x / w) for x in range(w)]
write("se_waves_far", w, h, shade(from_heights(w, h, hts), seed=47),
      [("a", "#1c3a6a"), ("b", "#6a9ad0")], "far waves")

w, h = 96, 22
hts = [14 + 4 * math.sin(6.283 * 3 * x / w) + 1.5 * math.sin(6.283 * 7 * x / w + 1) for x in range(w)]
write("se_waves_near", w, h, shade(from_heights(w, h, hts), seed=48),
      [("a", "#16305a"), ("b", "#9ac0f0")], "near waves")


# ---------------------------------------------------------------------------
# props with rows too long or too regular to type out

def write_frames(name, w, h, frames, palette, comment, origin, anims=()):
    path = os.path.join(OUT, name + ".pix")
    with open(path, "w", newline="\n") as f:
        f.write("// %s\n// made by tools/bggen.py, change it there\n" % comment)
        f.write("size %d %d\norigin %d %d\n" % (w, h, origin[0], origin[1]))
        if palette:
            f.write("palette\n")
            for ch, col in palette:
                f.write("%s %s\n" % (ch, col))
        for fname, rows in frames:
            assert len(rows) == h and all(len(r) == w for r in rows), fname
            f.write("\nframe %s\n" % fname)
            f.write("\n".join(rows) + "\n")
        for a in anims:
            f.write("\nanim %s\n" % a)
    print("wrote", path)


raft = [
    "tTTTTTTt" * 4,
    "TNNNNNNT" * 4,
    ("TNnNNNNT" + "TNNNnNNT") * 2,
    "t" * 32,
    "k" * 32,
    ".." + "qqqq...." * 3 + "qqqq..",
    "." * 32,
    "." * 32,
]
write_frames("raft", 32, 8, [("0", raft)], [("q", "#1c3f6e")], "raft, floats and drifts", (16, 0))

boat = []
for y in range(24):
    row = ["."] * 48
    if y >= 2:
        row[23] = row[24] = "t"
    if 3 <= y <= 20:
        width = int((y - 3) * 0.95)
        for x in range(25, 25 + width):
            row[x] = "w" if (x + y) % 7 else "L"
    boat.append("".join(row))
boat += [
    "T" * 48,
    "t" + "N" * 46 + "t",
    "k" + "t" * 46 + "k",
    ".k" + "R" * 44 + "k.",
    ".k" + "r" * 44 + "k.",
    "..k" + "w" * 42 + "k..",
    "..k" + "r" * 42 + "k..",
    "...k" + "R" * 40 + "k...",
    "....kk" + "R" * 36 + "kk....",
    "......" + "k" * 36 + "......",
]
boat += ["." * 48] * (40 - len(boat))
write_frames("boat", 48, 40, [("0", boat)], None, "boat, the deck is the origin row", (24, 24))

portal_frames = []
W, H = 24, 32
cx, cy = 11.5, 17.0
for f in range(4):
    rows = []
    for y in range(H):
        row = ""
        for x in range(W):
            dx, dy = (x - cx) / 11.5, (y - cy) / 15.5
            d = math.hypot(dx, dy)
            if y >= H - 2:
                row += "k" if y == H - 1 or abs(x - cx) > 10 else "S"
            elif d > 1:
                row += "."
            elif d > 0.93:
                row += "k"
            elif d > 0.78:
                row += "l" if dx < -0.2 and dy < 0 else ("s" if dx > 0.3 else "S")
            else:
                a = math.atan2(dy, dx) / 6.283
                v = (a * 3 + d * 2.5 - f / 4.0) % 1
                row += "w" if d < 0.14 else ("P" if v < 0.3 else "p" if v < 0.6 else "c" if v < 0.8 else "K")
        rows.append(row)
    portal_frames.append((str(f), rows))
write_frames("portal", W, H, portal_frames, None, "portal between worlds", (12, 32), ["spin 10 loop 0-3"])


# ---------------------------------------------------------------------------
# ice: snowy peaks, ice spires, frozen pines, snow clouds

def pines(w, h, count, seed, width=(10, 16), tall=(0.5, 0.9), jitter=0.35):
    """Triangles on a short trunk, stacked in tiers like a fir."""
    rnd = random.Random(seed)
    grid = [[False] * w for _ in range(h)]
    for i in range(count):
        cx = (i + rnd.uniform(-jitter, jitter)) * w / count
        top = h - int(h * rnd.uniform(*tall))
        half = rnd.randint(*width) / 2
        for y in range(max(0, top), h):
            k = (y - top) / max(1, h - top)
            # three tiers, each flaring out then pulled back in
            tier = (k * 3) % 1
            r = half * (0.35 + 0.65 * k) * (0.55 + 0.45 * tier)
            if y > h - 6:
                r = 1.5
            for x in range(int(cx - r), int(cx + r) + 1):
                grid[y][x % w] = True
    return grid


w, h = 200, 80
hts = [a + b + 18 for a, b in zip(ridge(w, [(1, 6), (3, 4)], 50), spikes(w, 4, 30, 58, 51, 0.3, power=1.6))]
rows = [list(r) for r in shade(from_heights(w, h, hts), seed=52)]
# snow on the upper half of each peak
for x in range(w):
    top = h - int(round(hts[x]))
    for y in range(max(0, top), min(h, top + int(hts[x] * 0.35))):
        if rows[y][x] == "a":
            rows[y][x] = "s"
write("gl_far", w, h, ["".join(r) for r in rows],
      [("a", "#1a2238"), ("b", "#3a4a6c"), ("s", "#5a6a8c")], "snowy peaks")

w, h = 160, 100
hts = [a + b + 12 for a, b in zip(ridge(w, [(1, 5), (4, 3)], 53), spikes(w, 6, 7, 78, 54, 0.3, power=0.9))]
write("gl_mid", w, h, shade(from_heights(w, h, hts), speckle=("c", 0.004), seed=55),
      [("a", "#142038"), ("b", "#3c5c88"), ("c", "#9ad0ff")], "ice spires")

w, h = 128, 130
write("gl_near", w, h, shade(pines(w, h, 3, 56, width=(22, 30), tall=(0.7, 0.95)), seed=57),
      [("a", "#0c1424"), ("b", "#44587a")], "frozen firs")

w, h = 192, 36
hts = [max(0, a + 11) for a in ridge(w, [(3, 5), (7, 3), (13, 2)], 58)]
write("gl_clouds", w, h, shade(from_heights(w, h, hts, True), from_top=True, seed=59),
      [("a", "#1c2438"), ("b", "#2e3a56")], "snow clouds")


# ice tiles: frozen rock under a snow cap, and clear slippery ice
def texture(seed, base, spots, rate):
    rnd = random.Random(seed)
    return [[spots[rnd.randrange(len(spots))] if rnd.random() < rate else base for _ in range(16)]
            for _ in range(16)]


def snowcap(body, seed):
    rnd = random.Random(seed)
    rows = [r[:] for r in body]
    edge = [3 + int(1.5 * math.sin(x * 0.9 + seed) + rnd.random()) for x in range(16)]
    for x in range(16):
        for y in range(edge[x] + 1):
            rows[y][x] = "A" if y == 0 else ("z" if y == edge[x] else "a")
    return rows


def ice(seed, top):
    rows = texture(seed, "i", ["I", "j"], 0.06)
    # a couple of diagonal shines
    for d in (3, 9):
        for k in range(4):
            y, x = d + k, 12 - d // 2 - k
            if 0 <= x < 16 and 0 <= y < 16:
                rows[y][x] = "J"
    if top:
        rows[0] = ["J"] * 16
        rows[1] = ["I"] * 16
    return rows


def join(rows):
    return ["".join(r) for r in rows]


rock_a = texture(60, "e", ["E", "h", "E"], 0.12)
rock_b = texture(61, "e", ["E", "h", "I"], 0.1)
crystal = [["."] * 16 for _ in range(16)]
for x0, hgt in ((3, 5), (6, 8), (8, 6), (12, 4)):
    for y in range(16 - hgt, 16):
        w_ = 1 if y < 16 - hgt + 2 else 2
        for dx in range(w_):
            crystal[y][x0 + dx] = "J" if dx == 0 else "i"
icicles = [["."] * 16 for _ in range(16)]
for x0, hgt in ((2, 9), (6, 5), (9, 12), (13, 7)):
    for y in range(hgt):
        wid = 2 if y < hgt * 0.5 else 1
        for dx in range(wid):
            icicles[y][x0 + dx] = "J" if dx == 0 else "i"
write_frames("tiles_ice", 16, 16,
             [("rock_a", join(rock_a)), ("rock_b", join(rock_b)), ("rock_top", join(snowcap(rock_a, 62))),
              ("ice_a", join(ice(63, False))), ("ice_top", join(ice(63, True))),
              ("crystal", join(crystal)), ("icicles", join(icicles))],
             [("a", "#dce6f2"), ("A", "#ffffff"), ("z", "#9eb2cc"), ("e", "#34405c"), ("E", "#46547a"),
              ("h", "#262e46"), ("i", "#6ea8dc"), ("I", "#a8d4f4"), ("j", "#3e72ae"), ("J", "#e8f6ff")],
             "the ice world: frozen rock under snow, clear slippery ice", (0, 0))


# a block of ice she can melt, whole then two stages of going
def block(w, h, eaten, seed):
    rnd = random.Random(seed)
    rows = []
    for y in range(h):
        row = ""
        for x in range(w):
            # melting eats in from the top, rounder at the corners
            corner = max(0, abs(x - (w - 1) / 2) - (w / 2 - 3)) * 1.2
            if y < eaten + corner:
                row += "C" if y + 1 >= eaten + corner and rnd.random() < 0.3 else "."
            elif x in (0, w - 1) or y == h - 1:
                row += "j"
            elif y < eaten + corner + 1:
                row += "J"
            elif (x + y) % 7 == 0 and y < h - 3:
                row += "J"
            else:
                row += "I" if rnd.random() < 0.15 else "i"
        rows.append(row)
    return rows


pal = [("i", "#6ea8dc"), ("I", "#a8d4f4"), ("j", "#3e72ae"), ("J", "#e8f6ff")]
write_frames("ice_block", 16, 16, [("whole", block(16, 16, 0, 70)), ("m1", block(16, 16, 4, 71)),
                                   ("m2", block(16, 16, 9, 72))],
             pal, "ice block that melts under her", (8, 0),
             ["whole 1 once whole", "m1 1 once m1", "m2 1 once m2"])
write_frames("ice_wall", 16, 48, [("whole", block(16, 48, 0, 73)), ("m1", block(16, 48, 14, 74)),
                                  ("m2", block(16, 48, 30, 75))],
             pal, "wall of ice, melts or cracks under arrows", (8, 48),
             ["whole 1 once whole", "m1 1 once m1", "m2 1 once m2"])

stal = []
for y in range(16):
    half = max(0.5, 3.6 * (1 - y / 16.0))
    row = ""
    for x in range(8):
        d = x - 3.5
        if abs(d) > half:
            row += "."
        elif d < -half + 1.2:
            row += "J"
        else:
            row += "i" if abs(d) < half - 1 else "j"
    stal.append(row)
write_frames("stalactite", 8, 16, [("0", stal)], pal, "falls when she walks under", (4, 5))


# ---------------------------------------------------------------------------
# sky: dawn, floating islands, cloud banks

def puffs(w, h, count, seed, r=(6, 12), base=0.7):
    """Round blobs sitting on a line, like the top of a cloud bank."""
    rnd = random.Random(seed)
    grid = [[False] * w for _ in range(h)]
    floor = int(h * base)
    for y in range(floor, h):
        for x in range(w):
            grid[y][x] = True
    for i in range(count):
        cx = (i + rnd.uniform(-0.4, 0.4)) * w / count
        rr = rnd.uniform(*r)
        cy = floor - rr * rnd.uniform(0.1, 0.6)
        for y in range(max(0, int(cy - rr)), h):
            for x in range(int(cx - rr), int(cx + rr) + 1):
                if (x - cx) ** 2 + (y - cy) ** 2 <= rr * rr:
                    grid[y][x % w] = True
    return grid


def islands(w, h, count, seed, size=(14, 26)):
    """Chunks of rock hanging in the air: flat top, pointed underside."""
    rnd = random.Random(seed)
    grid = [[False] * w for _ in range(h)]
    for i in range(count):
        cx = (i + rnd.uniform(-0.3, 0.3)) * w / count
        half = rnd.uniform(*size) / 2
        top = rnd.randint(int(h * 0.15), int(h * 0.5))
        depth = half * rnd.uniform(1.2, 2.0)
        for y in range(top, min(h, int(top + depth))):
            k = (y - top) / depth
            r = half * (1 - k ** 1.5)
            for x in range(int(cx - r), int(cx + r) + 1):
                grid[y][x % w] = True
    return grid


w, h = 192, 48
write("sk_high", w, h, shade(puffs(w, h, 7, 90, r=(5, 10), base=1.0), seed=91),
      [("a", "#6a5a8c"), ("b", "#9a80b0")], "thin clouds up high")

w, h = 200, 60
write("sk_far", w, h, shade(puffs(w, h, 9, 92, r=(8, 16), base=0.6), speckle=("c", 0.003), seed=93),
      [("a", "#c89aa8"), ("b", "#f0c8c0"), ("c", "#fff0e0")], "far cloud bank")

w, h = 180, 110
write("sk_mid", w, h, shade(islands(w, h, 3, 94), seed=95),
      [("a", "#5a4a70"), ("b", "#8a70a0")], "floating islands")

w, h = 160, 50
write("sk_near", w, h, shade(puffs(w, h, 5, 96, r=(12, 20), base=0.55), seed=97),
      [("a", "#e8c8d0"), ("b", "#ffffff")], "near clouds")


# tiles: cloud you can land on from above, floating stone
def cloud_tile(seed):
    rnd = random.Random(seed)
    rows = []
    for y in range(16):
        row = ""
        for x in range(16):
            bump = 2 + 1.5 * math.sin(x * 0.8 + seed)
            if y < bump:
                row += "."
            elif y < bump + 1:
                row += "A"
            elif y > 11:
                row += "." if (x + y) % 3 == 0 or y > 13 else "z"
            else:
                row += "a" if rnd.random() > 0.08 else "A"
        rows.append(row)
    return rows


stone_a = texture(100, "e", ["E", "h"], 0.12)
stone_b = texture(101, "e", ["E", "h", "E"], 0.1)
stone_top = [r[:] for r in stone_a]
for x in range(16):
    grass = 2 + (x * 7 % 3 == 0)
    for y in range(grass + 1):
        stone_top[y][x] = "g" if y < grass else "G"
    if x % 5 == 2:
        stone_top[0][x] = "."
flowers = [["."] * 16 for _ in range(16)]
for x0, ch in ((2, "y"), (7, "w"), (11, "y"), (14, "p")):
    for y in range(12, 16):
        flowers[y][x0] = "g"
    flowers[11][x0] = ch
write_frames("tiles_sky", 16, 16,
             [("cloud", cloud_tile(3)), ("stone_a", join(stone_a)), ("stone_b", join(stone_b)),
              ("stone_top", join(stone_top)), ("flowers", join(flowers))],
             [("a", "#fff4f0"), ("A", "#ffffff"), ("z", "#e0c8d8"), ("e", "#6a5a78"), ("E", "#80709a"),
              ("h", "#4a3e58"), ("g", "#6bb04c"), ("G", "#3f7a3a")],
             "the sky: clouds to land on, floating stone", (0, 0))

# rain cloud
rows = []
for y in range(16):
    row = ""
    for x in range(48):
        d = min(1, ((x - 24) / 24) ** 2 + ((y - 9) / 7) ** 2)
        bump = 3 * math.sin(x * 0.45) + 2 * math.sin(x * 1.3)
        if y < 5 - bump * 0.5 and d > 0.4 or d >= 1:
            row += "."
        elif y > 12:
            row += "K"
        else:
            row += "S" if (x * 3 + y * 5) % 11 else "l"
    rows.append(row)
write_frames("raincloud", 48, 16, [("0", rows)], None, "rain cloud", (24, 8))

# windmill: a post and four sails, the sails turn
def mill(angle):
    W, H = 32, 44
    cx, cy = 15.5, 14.5
    g = [["."] * W for _ in range(H)]
    for y in range(18, H):
        half = 2 + (y - 18) * 0.12
        for x in range(int(cx - half), int(cx + half) + 1):
            g[y][x] = "T" if abs(x - cx) < half - 1 else "t"
    for k in range(4):
        a = angle + k * math.pi / 2
        for r in range(2, 15):
            for side in range(-1, 3 if r > 5 else 1):
                px = cx + math.cos(a) * r - math.sin(a) * side
                py = cy + math.sin(a) * r + math.cos(a) * side
                if 0 <= int(py) < H and 0 <= int(px) < W:
                    g[int(py)][int(px)] = "w" if side > 0 else "N"
    g[int(cy)][int(cx)] = "k"
    return ["".join(r) for r in g]


write_frames("windmill", 32, 44, [(str(f), mill(f * math.pi / 8)) for f in range(4)], None,
             "windmill, an arrow starts or stops it", (16, 44), ["still 1 once 0", "spin 10 loop 0-3"])

bird = [
    ["............",
     "..kk....kk..",
     ".kwwk..kwwk.",
     "..kwwkkwwk..",
     "....kOOk....",
     "...kwwwwyk..",
     "....kkkk....",
     "............"],
    ["............",
     "............",
     "............",
     "....kkkk....",
     "..kwwOOwwk..",
     ".kwwkwwwwyk.",
     "kwk..kkkk.kk",
     ".k........k."],
]
write_frames("bird", 12, 8, [("0", bird[0]), ("1", bird[1])], None, "sky bird", (6, 4), ["fly 8 loop 0-1"])


# ---------------------------------------------------------------------------
# the Froid: a big faceted crystal with a face, and once its shield is
# gone, cracks with fire showing through

def froid(frame, weak):
    W = H = 48
    cx, cy = 23.5, 24.0
    rnd = random.Random(200 + frame)
    rows = []
    for y in range(H):
        row = ""
        for x in range(W):
            dx, dy = (x - cx) / 21, (y - cy) / 23
            # a diamond, pinched at the sides, with a wobble so it shimmers
            d = abs(dx) * (1.1 + 0.08 * math.sin(y * 0.5 + frame * 3)) + abs(dy)
            if d > 1:
                row += "."
                continue
            if d > 0.93:
                row += "j"
            elif (x + y + frame) % 9 == 0 or (x - y) % 11 == 0:
                row += "J"
            elif d < 0.45:
                row += "I"
            else:
                row += "i"
        rows.append(list(row))
    # eyes and a mouth, dark and hollow
    for ex in (17, 29):
        for y in range(19, 24):
            for x in range(ex - 2, ex + 2):
                rows[y][x] = "d" if not (y == 20 and x == ex - 1) else "C"
    for x in range(19, 29):
        yy = 31 + int(1.5 * math.sin((x - 19) / 9 * math.pi))
        rows[yy][x] = "d"
        rows[yy + 1][x] = "d"
    if weak:
        # cracks running out from the middle, glowing
        for k in range(6):
            a = k * 1.05 + 0.3 + frame * 0.1
            x, y = cx, cy
            for step in range(20):
                x += math.cos(a) + rnd.uniform(-0.6, 0.6)
                y += math.sin(a) + rnd.uniform(-0.6, 0.6)
                ix, iy = int(x), int(y)
                if 0 <= ix < W and 0 <= iy < H and rows[iy][ix] not in ".d":
                    rows[iy][ix] = "y" if step < 6 else "o"
    return ["".join(r) for r in rows]


write_frames("froid", 48, 48,
             [("idle0", froid(0, False)), ("idle1", froid(1, False)),
              ("weak0", froid(0, True)), ("weak1", froid(1, True))],
             [("i", "#6ea8dc"), ("I", "#c8e8ff"), ("j", "#2e5c94"), ("J", "#ffffff")],
             "the Froid", (24, 24), ["idle 4 loop idle0 idle1", "weak 6 loop weak0 weak1"])

bolt = []
for f in range(2):
    rows = []
    for y in range(8):
        row = ""
        for x in range(8):
            d = math.hypot(x - 3.5, y - 3.5)
            spike = (x + y + f) % 2 == 0 and d < 4.2
            row += "J" if d < 1.5 else "I" if d < 2.6 else "i" if d < 3.4 or spike else "."
        rows.append(row)
    bolt.append((str(f), rows))
write_frames("frostbolt", 8, 8, bolt, [("i", "#6ea8dc"), ("I", "#c8e8ff"), ("J", "#ffffff")],
             "the Froid throws these", (4, 4), ["spin 10 loop 0-1"])


# ---------------------------------------------------------------------------
# the forge: a ruined city of chimneys, gears and girders, glowing from below

def chimneys(w, h, count, seed):
    rnd = random.Random(seed)
    hts = [0.0] * w
    x = 0
    while x < w:
        bw = rnd.randint(8, 22)
        bh = rnd.uniform(h * 0.25, h * 0.6)
        for i in range(x, min(w, x + bw)):
            hts[i] = bh
        # a stack on some roofs
        if rnd.random() < 0.6:
            cx = x + rnd.randint(1, max(1, bw - 4))
            ch = bh + rnd.uniform(h * 0.15, h * 0.35)
            for i in range(cx, min(w, cx + 3)):
                hts[i] = ch
        x += bw + rnd.randint(0, 4)
    return hts


w, h = 200, 80
hts = chimneys(w, h, 12, 110)
rows = [list(r) for r in shade(from_heights(w, h, hts), seed=111)]
# lit windows
rnd = random.Random(112)
for _ in range(60):
    x, y = rnd.randrange(w), rnd.randrange(h)
    if rows[y][x] == "a" and y > 4:
        rows[y][x] = "o"
write("fr_far", w, h, ["".join(r) for r in rows],
      [("a", "#1e1418"), ("b", "#3a2426"), ("o", "#c86a30")], "chimneys of the forge city")


def gears(w, h, count, seed):
    rnd = random.Random(seed)
    grid = [[False] * w for _ in range(h)]
    for i in range(count):
        cx = (i + rnd.uniform(-0.3, 0.3)) * w / count
        cy = rnd.uniform(h * 0.3, h * 0.8)
        r = rnd.uniform(14, 26)
        teeth = int(r * 0.6)
        for y in range(h):
            for x in range(int(cx - r - 4), int(cx + r + 5)):
                dx, dy = x - cx, y - cy
                d = math.hypot(dx, dy)
                a = math.atan2(dy, dx)
                rim = r + (3 if math.sin(a * teeth) > 0.3 else 0)
                if d <= rim and not (r * 0.3 < d < r * 0.55 and math.sin(a * 5) > 0.2):
                    grid[y][x % w] = True
        for y in range(int(cy), h):
            for x in range(int(cx - 2), int(cx + 3)):
                grid[y][x % w] = True
    return grid


w, h = 180, 110
write("fr_mid", w, h, shade(gears(w, h, 3, 113), seed=114),
      [("a", "#1a1012"), ("b", "#4a2a22")], "great gears")

w, h = 128, 130
grid = [[False] * w for _ in range(h)]
for x in range(w):
    for y in range(h):
        # girders: two posts and a cross brace, pipes along the bottom
        post = x % 64 in range(4, 10) or x % 64 in range(40, 46)
        brace = abs((x % 64) - (y % 64)) < 3 and y > 30
        pipe = h - 20 < y < h - 12
        if post and y > 20 or brace or pipe:
            grid[y][x] = True
write("fr_near", w, h, shade(grid, seed=115), [("a", "#100a0c"), ("b", "#3a221c")], "girders and pipes")

w, h = 192, 40
hts = [max(0, a + 14) for a in ridge(w, [(2, 6), (5, 4), (11, 2)], 116)]
write("fr_smoke", w, h, shade(from_heights(w, h, hts, True), from_top=True, seed=117),
      [("a", "#221618"), ("b", "#3a2426")], "soot")


# tiles
metal_a = texture(120, "e", ["E", "h"], 0.05)
for y in (0, 15):
    for x in range(16):
        metal_a[y][x] = "h"
for y in range(16):
    metal_a[y][0] = "E"
for x, y in ((2, 2), (13, 2), (2, 13), (13, 13)):
    metal_a[y][x] = "L"
metal_b = [r[:] for r in metal_a]
for x in range(3, 13):
    metal_b[7][x] = "h"
    metal_b[8][x] = "E"
metal_top = [r[:] for r in metal_a]
for x in range(16):
    metal_top[0][x] = "L"
    metal_top[1][x] = "E"
    if x % 4 == 1:
        metal_top[2][x] = "o"
grate = [["."] * 16 for _ in range(16)]
for x in range(16):
    grate[0][x] = "L"
    grate[1][x] = "E"
    grate[3][x] = "h"
for x in range(0, 16, 3):
    grate[2][x] = "E"
belts = []
for f in range(4):
    b = [["."] * 16 for _ in range(16)]
    for y in range(16):
        for x in range(16):
            if y < 3:
                b[y][x] = "k" if (x + f * 2) % 4 == 0 else "s"
            elif y == 3:
                b[y][x] = "h"
            else:
                b[y][x] = metal_a[y][x]
    belts.append(b)
pipes = [["."] * 16 for _ in range(16)]
for y in range(16):
    for x in (6, 7, 8, 9):
        pipes[y][x] = "E" if x in (6, 9) else "L"
    if y % 7 == 3:
        for x in range(5, 11):
            pipes[y][x] = "h"
write_frames("tiles_forge", 16, 16,
             [("metal_a", join(metal_a)), ("metal_b", join(metal_b)), ("metal_top", join(metal_top)),
              ("grate", join(grate)), ("belt0", join(belts[0])), ("belt1", join(belts[1])),
              ("belt2", join(belts[2])), ("belt3", join(belts[3])), ("pipes", join(pipes))],
             [("e", "#4a3a3a"), ("E", "#6a5450"), ("h", "#2a1e20"), ("L", "#a08a80"), ("s", "#3a3030")],
             "the forge: plated metal, catwalks, belts, pipes", (0, 0))


def box(w, h, fn):
    return ["".join(fn(x, y) for x in range(w)) for y in range(h)]


# lever: a post with a handle leaning left (off) or right (on)
def lever(on):
    def px(x, y):
        if y >= 12:
            return "k" if y == 15 or x in (2, 9) else "s"
        hx = 5 + (y - 11) * (-1 if on else 1) * 0.45
        if abs(x - hx) < 1:
            return "T"
        if y < 3 and abs(x - (5 + (4.5 if on else -4.5))) < 2:
            return "r"
        return "."
    return box(12, 16, px)


write_frames("lever", 12, 16, [("off", lever(False)), ("on", lever(True))], None,
             "lever, strike it", (6, 16), ["off 1 once off", "on 1 once on"])

gate_shut = box(16, 48, lambda x, y: "k" if y in (0, 1, 47) else ("L" if x % 5 == 2 else ("S" if x % 5 == 3 else ".")))
gate_open = box(16, 48, lambda x, y: ("k" if y in (0, 1) else ("L" if x % 5 == 2 else "S" if x % 5 == 3 else ".")) if y < 6 else ".")
write_frames("gate", 16, 48, [("shut", gate_shut), ("open", gate_open)], None, "gate", (8, 48),
             ["shut 1 once shut", "open 1 once open"])

press = box(32, 40, lambda x, y: "k" if x in (0, 31) or y in (0, 39) else
            ("r" if y > 33 and (x // 4) % 2 == 0 else ("S" if y > 33 else ("l" if x in (1, 2) or y == 1 else "s"))))
write_frames("press", 32, 40, [("0", press)], None, "press, slams down", (16, 0))


def saw(f):
    def px(x, y):
        d = math.hypot(x - 7.5, y - 7.5)
        a = math.atan2(y - 7.5, x - 7.5) + f * 0.4
        if d < 2:
            return "k"
        if d < 5.5:
            return "L"
        if d < 7.8 and math.sin(a * 8) > -0.2:
            return "l" if d < 6.8 else "S"
        return "."
    return box(16, 16, px)


write_frames("saw", 16, 16, [(str(f), saw(f)) for f in range(3)], None, "saw blade", (8, 8), ["spin 16 loop 0-2"])

planks = box(16, 32, lambda x, y: "k" if x in (0, 15) else ("t" if y % 8 in (0, 7) else ("n" if (x + y // 8) % 5 == 0 else "T")))
write_frames("fragile", 16, 32, [("0", planks)], None, "rotten planks, dash through", (8, 32))

# furnace: a brick box with a mouth, cold or roaring
def furnace(lit, f):
    def px(x, y):
        if y < 4 and 9 <= x <= 14:
            return "k" if x in (9, 14) else "s"
        if y < 4:
            return "."
        if x in (0, 23) or y in (4, 23):
            return "k"
        mouth = 6 <= x <= 17 and 12 <= y <= 20
        if mouth:
            if not lit:
                return "d"
            v = (x * 3 + y * 5 + f * 7) % 11
            return "Y" if v < 2 else "y" if v < 5 else "o" if v < 8 else "O"
        return "R" if (x // 4 + y // 3) % 2 == 0 else "r"
    return box(24, 24, px)


write_frames("furnace", 24, 24, [("cold", furnace(False, 0)), ("lit0", furnace(True, 0)), ("lit1", furnace(True, 1))],
             None, "furnace, a fire arrow lights it", (12, 24), ["cold 1 once cold", "lit 8 loop lit0 lit1"])

auto0 = ["..............",
         ".....kkkk.....",
         "....kSSSSk....",
         "...kSoSSoSk...",
         "...kSSSSSSk...",
         "..kLLLLLLLLk..",
         "..kLsLLLLsLk..",
         "..kLLLLLLLLk..",
         "...kkkkkkkk...",
         "...kS....Sk...",
         "..kS......Sk..",
         "..kk......kk.."]
auto1 = auto0[:9] + ["....kS..Sk....", "....kS..Sk....", "....kk..kk...."]
write_frames("automaton", 14, 12, [("walk0", auto0), ("walk1", auto1)], None, "clockwork guard",
             (7, 12), ["walk 6 loop walk0 walk1"])


# ---------------------------------------------------------------------------
# the shadows: a land with no light of its own. Black spires with a violet
# edge, mist, and whatever the Night is made of

w, h = 200, 90
hts = [a + b + 10 for a, b in zip(ridge(w, [(1, 5), (3, 3)], 130), spikes(w, 8, 6, 70, 131, 0.3, power=1.4))]
write("om_far", w, h, shade(from_heights(w, h, hts), speckle=("c", 0.002), seed=132),
      [("a", "#0c0a14"), ("b", "#241a3a"), ("c", "#6a4aa0")], "black spires")

w, h = 160, 110
write("om_mid", w, h, shade(trees(w, h, 4, 133, trunk=(2, 3), crown=(6, 10), tall=(0.5, 0.85), jitter=0.4),
                            speckle=("c", 0.003), seed=134),
      [("a", "#08060e"), ("b", "#2a1e44"), ("c", "#8a6ad0")], "dead trees")

w, h = 128, 130
hts = [a + b + 6 for a, b in zip(ridge(w, [(2, 4)], 135), spikes(w, 3, 14, 110, 136, 0.2, power=0.5))]
write("om_near", w, h, shade(from_heights(w, h, hts), seed=137), [("a", "#040308"), ("b", "#1a1428")], "near rocks")

w, h = 192, 40
hts = [max(0, a + 12) for a in ridge(w, [(2, 5), (5, 4), (11, 2)], 138)]
write("om_mist", w, h, shade(from_heights(w, h, hts, True), from_top=True, seed=139),
      [("a", "#100c1a"), ("b", "#1c1630")], "mist")

# tiles
obs_a = texture(140, "e", ["E", "h"], 0.08)
obs_b = texture(141, "e", ["E", "h", "c"], 0.08)
obs_top = [r[:] for r in obs_a]
for x in range(16):
    obs_top[0][x] = "c"
    obs_top[1][x] = "E"
crystals = [["."] * 16 for _ in range(16)]
for x0, hgt in ((3, 6), (5, 9), (7, 5), (12, 7)):
    for y in range(16 - hgt, 16):
        crystals[y][x0] = "C" if y < 16 - hgt + 2 else "c"
write_frames("tiles_shadow", 16, 16,
             [("obs_a", join(obs_a)), ("obs_b", join(obs_b)), ("obs_top", join(obs_top)), ("crystals", join(crystals))],
             [("e", "#16121e"), ("E", "#241c32"), ("h", "#0c0a12"), ("c", "#6a4aa0"), ("C", "#c8a8ff")],
             "the shadows: obsidian and violet crystals", (0, 0))

plat_solid = ["cCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCc",
              "cEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEc",
              "ceeeeeeeeeeeeeeeeeeeeeeeeeeeeeec",
              ".ceeeeeeeeeeeeeeeeeeeeeeeeeeeec.",
              "..cccccccccccccccccccccccccccc..",
              "................................"]
plat_faint = ["c...c...c...c...c...c...c...c...",
              "................................",
              "..c...c...c...c...c...c...c...c.",
              "................................",
              "................................",
              "................................"]
write_frames("shadow_plat", 32, 6, [("solid", plat_solid), ("faint", plat_faint)],
             [("e", "#241c32"), ("E", "#3a2c52"), ("c", "#6a4aa0"), ("C", "#c8a8ff")],
             "only there in the light", (16, 0), ["solid 1 once solid", "faint 1 once faint"])


def wisp(f):
    rnd = random.Random(150 + f)
    rows = []
    for y in range(16):
        row = ""
        for x in range(16):
            d = math.hypot((x - 7.5) / 6, (y - 6.5 - f * 0.5) / 6)
            tail = y > 9 and abs(x - 7.5 + math.sin(y + f * 2) * 2) < (16 - y) * 0.5
            if d < 1 or tail:
                row += "k" if rnd.random() < 0.85 else "d"
            else:
                row += "."
        rows.append(list(row))
    for ex in (5, 10):
        rows[6][ex] = "C"
    return ["".join(r) for r in rows]


write_frames("shade", 16, 16, [("0", wisp(0)), ("1", wisp(1))], [("C", "#c8a8ff")], "a scrap of shadow", (8, 8),
             ["float 5 loop 0-1"])

def lantern(lit, f=0):
    def px(x, y):
        if y < 2:
            return "k" if 4 <= x <= 7 else "."
        if y == 2 or y == 17:
            return "k" if 1 <= x <= 10 else "."
        if y > 17:
            return "k" if 5 <= x <= 6 else "."
        if x in (1, 10):
            return "k"
        if 2 <= x <= 9:
            if not lit:
                return "d"
            v = (x * 3 + y * 7 + f * 5) % 9
            return "Y" if v < 3 else "y" if v < 6 else "O"
        return "."
    return box(12, 20, px)


write_frames("lantern", 12, 20, [("cold", lantern(False)), ("lit0", lantern(True, 0)), ("lit1", lantern(True, 1))],
             None, "lantern, a fire arrow lights it", (6, 20), ["cold 1 once cold", "lit 6 loop lit0 lit1"])


def night(f, stunned):
    W, H = 64, 48
    rnd = random.Random(160 + f)
    rows = []
    for y in range(H):
        row = ""
        for x in range(W):
            # a hooded blob with ragged edges and arms trailing off
            dx, dy = (x - 31.5) / 22, (y - 20) / 18
            d = math.hypot(dx, dy)
            edge = 1 + 0.12 * math.sin(x * 0.7 + f * 2) + 0.1 * math.sin(y * 0.9)
            arm = y > 18 and abs(abs(x - 31.5) - (y - 18) * 0.9 - 14) < 3 - (y - 18) * 0.08
            if d < edge or arm:
                if stunned and rnd.random() < 0.12:
                    row += "C"
                else:
                    row += "k" if rnd.random() < 0.8 else "d"
            else:
                row += "."
        rows.append(list(row))
    # a violet rim so she can be told apart from the dark around her
    inside = [[ch != "." for ch in r] for r in rows]
    for y in range(H):
        for x in range(W):
            if inside[y][x] and any(not (0 <= y + dy < H and 0 <= x + dx < W and inside[y + dy][x + dx])
                                    for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                rows[y][x] = "c"
    eye = "w" if stunned else "C"
    for ex in (24, 39):
        for y in range(16, 20):
            for x in range(ex - 2, ex + 2):
                rows[y][x] = eye
    return ["".join(r) for r in rows]


write_frames("nuit", 64, 48, [("idle0", night(0, False)), ("idle1", night(1, False)), ("stun", night(0, True))],
             [("C", "#c8a8ff"), ("c", "#6a4aa0")], "the Night", (32, 24), ["idle 4 loop idle0 idle1", "stun 1 once stun"])

sbolt = []
for f in range(2):
    rows = []
    for y in range(8):
        row = ""
        for x in range(8):
            d = math.hypot(x - 3.5, y - 3.5)
            row += "C" if d < 1.2 else "k" if d < 3.2 or ((x + y + f) % 3 == 0 and d < 4) else "."
        rows.append(row)
    sbolt.append((str(f), rows))
write_frames("shadowbolt", 8, 8, sbolt, [("C", "#c8a8ff")], "the Night throws these", (4, 4), ["spin 10 loop 0-1"])


def sun(size, face=True):
    c = (size - 1) / 2
    rows = []
    for y in range(size):
        row = ""
        for x in range(size):
            dx, dy = x - c, y - c
            d = math.hypot(dx, dy)
            a = math.atan2(dy, dx)
            r = size * 0.3
            if d < r - 1.5:
                row += "Y" if d < r * 0.6 else "y"
            elif d < r:
                row += "O"
            elif d < size * 0.48 and math.sin(a * 12) > 0.55:
                row += "o" if d < size * 0.4 else "O"
            else:
                row += "."
        rows.append(list(row))
    if face and size >= 24:
        for ex in (c - size * 0.1, c + size * 0.1):
            for yy in range(int(c - size * 0.08), int(c - size * 0.02)):
                rows[yy][int(ex)] = "k"
        for x in range(int(c - size * 0.1), int(c + size * 0.1) + 1):
            yy = int(c + size * 0.1 + 1.5 * math.sin((x - c + size * 0.1) / (size * 0.2) * math.pi))
            rows[yy][x] = "k"
    return ["".join(r) for r in rows]


write_frames("soleil", 48, 48, [("0", sun(48))], None, "Lueur, at the very end", (24, 24))
write_frames("sun_icon", 12, 12, [("0", sun(12, False))], None, "hud: the burst is ready", (6, 6))
