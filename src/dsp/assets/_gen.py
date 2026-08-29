"""Generate 32x32 pixel-art assets for Dying Sun Procession."""

from __future__ import annotations

import json
import math
from pathlib import Path

import numpy as np
from PIL import Image

OUT = Path(__file__).resolve().parent
SRC = Path(
    r"C:\Users\jackw\.cursor\projects"
    r"\c-Users-jackw-OneDrive-Desktop-git-projects-aphelion-engine-ap2"
    r"\assets"
)

W = 32
COLS = 16
ROWS = 16

# Dying-sun palette (RGBA)
VOID = (10, 8, 16, 255)
INK = (18, 14, 22, 255)
CRACK = (28, 24, 32, 255)
HOOD = (24, 18, 20, 255)
ROBE = (48, 32, 28, 255)
MANTLE = (110, 36, 28, 255)
MANTLE_DK = (72, 24, 20, 255)
ASH = (58, 52, 48, 255)
ASH_LT = (86, 78, 70, 255)
STONE = (72, 70, 68, 255)
STONE_LT = (104, 98, 92, 255)
STONE_DK = (46, 44, 48, 255)
DUST = (118, 104, 88, 255)
CLAY = (148, 108, 72, 255)
SAND = (168, 140, 100, 255)
BONE = (200, 187, 160, 255)
BONE_LT = (230, 214, 186, 255)
BLOOD = (90, 18, 18, 255)
BLOOD_LT = (148, 32, 28, 255)
RUST = (138, 42, 24, 255)
AMBER = (196, 92, 18, 255)
LANTERN = (240, 176, 64, 255)
GOLD = (212, 160, 74, 255)
GOLD_LT = (236, 196, 110, 255)
PURPLE = (58, 26, 74, 255)
VOID_GLOW = (112, 48, 140, 255)
COSMIC = (164, 78, 184, 255)
TEAL = (42, 78, 70, 255)
TEAL_LT = (78, 130, 108, 255)
FLESH = (132, 44, 52, 255)
EYE = (232, 226, 214, 255)
MAGMA = (220, 86, 24, 255)
NONE = (0, 0, 0, 0)

PALETTE_RGB = [
    VOID[:3], INK[:3], CRACK[:3], HOOD[:3], ROBE[:3], MANTLE[:3],
    MANTLE_DK[:3], ASH[:3], ASH_LT[:3], STONE[:3], STONE_LT[:3], STONE_DK[:3],
    DUST[:3], CLAY[:3], SAND[:3], BONE[:3], BONE_LT[:3], BLOOD[:3],
    BLOOD_LT[:3], RUST[:3], AMBER[:3], LANTERN[:3], GOLD[:3], GOLD_LT[:3],
    PURPLE[:3], VOID_GLOW[:3], COSMIC[:3], TEAL[:3], TEAL_LT[:3], FLESH[:3],
    EYE[:3], MAGMA[:3], (20, 16, 14), (36, 28, 26), (64, 22, 18),
    (88, 64, 52), (40, 36, 42), (16, 12, 18), (180, 70, 36), (252, 210, 120),
]


def blank(color=NONE) -> np.ndarray:
    img = np.zeros((W, W, 4), dtype=np.uint8)
    img[:, :] = color
    return img


def put(img: np.ndarray, x: int, y: int, c) -> None:
    if 0 <= x < W and 0 <= y < W:
        img[y, x] = c


def h2(x: int, y: int, seed: int) -> int:
    n = (x * 374761393 + y * 668265263 + seed * 1274126177) & 0xFFFFFFFF
    n = (n ^ (n >> 13)) * 1274126177
    return (n ^ (n >> 16)) & 0xFFFFFFFF


def n2(x: int, y: int, seed: int) -> int:
    return h2(x & 31, y & 31, seed) & 255


def noise(x: int, y: int, seed: int) -> int:
    s = 0
    for dy in range(-1, 2):
        for dx in range(-1, 2):
            s += n2(x + dx, y + dy, seed)
    return s // 9


def lerp_c(a, b, t: float):
    t = max(0.0, min(1.0, t))
    return tuple(
        max(0, min(255, int(int(a[i]) + (int(b[i]) - int(a[i])) * t)))
        for i in range(3)
    ) + (255,)


def texture_fill(img: np.ndarray, a, b, seed: int, contrast: float = 0.35) -> None:
    for y in range(W):
        for x in range(W):
            t = (noise(x, y, seed) / 255.0 - 0.5) * contrast + 0.5
            img[y, x] = lerp_c(a, b, t)


def sprinkle(img: np.ndarray, color, seed: int, chance: int) -> None:
    for y in range(W):
        for x in range(W):
            if n2(x, y, seed) < chance:
                img[y, x] = color


def cracks(img: np.ndarray, color, seed: int, count: int = 4) -> None:
    for i in range(count):
        x = h2(i, 3, seed) & 31
        y = h2(i, 9, seed) & 31
        for _ in range(10 + (h2(i, 1, seed) & 7)):
            put(img, x, y, color)
            d = h2(x, y, seed + i) % 4
            x = (x + (1, 0, -1, 0)[d]) & 31
            y = (y + (0, 1, 0, -1)[d]) & 31


def stain(img: np.ndarray, color, seed: int, cx: int, cy: int, rad: int) -> None:
    for y in range(W):
        for x in range(W):
            d = math.hypot(x - cx, y - cy)
            if d < rad and n2(x, y, seed) > 40:
                t = 1.0 - d / rad
                if t > 0.25:
                    img[y, x] = lerp_c(img[y, x], color, t * 0.7)


def rect(img: np.ndarray, x: int, y: int, w: int, h: int, c) -> None:
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            put(img, xx, yy, c)


def outline_nonzero(img: np.ndarray, color) -> None:
    src = img.copy()
    for y in range(W):
        for x in range(W):
            if src[y, x, 3] == 0:
                continue
            for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                nx, ny = x + dx, y + dy
                if not (0 <= nx < W and 0 <= ny < W) or src[ny, nx, 3] == 0:
                    put(img, x, y, color)
                    break


# ---------------------------------------------------------------------------
# Ground tiles
# ---------------------------------------------------------------------------

def tile_ash(seed: int) -> np.ndarray:
    img = blank(ASH)
    texture_fill(img, CRACK, ASH_LT, seed, 0.45)
    sprinkle(img, STONE_DK, seed + 2, 18)
    sprinkle(img, DUST, seed + 3, 10)
    cracks(img, INK, seed + 4, 3)
    return img


def tile_cracked_earth(seed: int) -> np.ndarray:
    img = blank(CLAY)
    texture_fill(img, ROBE, SAND, seed, 0.4)
    cracks(img, INK, seed, 6)
    cracks(img, CRACK, seed + 7, 4)
    sprinkle(img, AMBER, seed + 9, 6)
    return img


def tile_void_grass(seed: int) -> np.ndarray:
    img = blank(INK)
    texture_fill(img, VOID, PURPLE, seed, 0.5)
    for y in range(W):
        for x in range(W):
            if n2(x, y, seed) < 28:
                h = 2 + (n2(x, y, seed + 1) & 2)
                for i in range(h):
                    put(img, x, (y - i) & 31, TEAL if i else TEAL_LT)
    sprinkle(img, VOID_GLOW, seed + 4, 8)
    return img


def tile_cobble(seed: int) -> np.ndarray:
    img = blank(STONE_DK)
    for gy in range(0, W, 8):
        for gx in range(0, W, 8):
            ox = 0 if (gy // 8) % 2 == 0 else 4
            sx = (gx + ox) & 31
            col = lerp_c(STONE_DK, STONE_LT, (n2(gx, gy, seed) / 255.0) * 0.7 + 0.15)
            for yy in range(gy + 1, gy + 7):
                for xx in range(sx + 1, sx + 7):
                    put(img, xx & 31, yy & 31, col)
            for k in range(8):
                put(img, (sx + k) & 31, gy, INK)
                put(img, (sx + k) & 31, (gy + 7) & 31, INK)
                put(img, sx, (gy + k) & 31, INK)
    sprinkle(img, DUST, seed, 8)
    return img


def tile_ritual(seed: int) -> np.ndarray:
    img = tile_cobble(seed)
    cx, cy = 15, 15
    for y in range(W):
        for x in range(W):
            d = abs(math.hypot(x - cx, y - cy) - 10)
            if d < 1.2:
                img[y, x] = BLOOD_LT if n2(x, y, seed) > 80 else BLOOD
            d2 = abs(math.hypot(x - cx, y - cy) - 6)
            if d2 < 0.9:
                img[y, x] = GOLD
    # inner sun rays
    for a in range(8):
        ang = a * math.pi / 4
        for r in range(3, 7):
            put(img, int(cx + math.cos(ang) * r), int(cy + math.sin(ang) * r), GOLD)
    put(img, cx, cy, GOLD_LT)
    return img


def tile_blood_dirt(seed: int) -> np.ndarray:
    img = tile_ash(seed)
    stain(img, BLOOD, seed, 12, 18, 11)
    stain(img, BLOOD_LT, seed + 2, 20, 10, 6)
    cracks(img, BLOOD, seed + 4, 2)
    return img


def tile_bone_ground(seed: int) -> np.ndarray:
    img = tile_ash(seed)
    for i in range(7):
        x = 4 + (h2(i, 0, seed) % 24)
        y = 4 + (h2(i, 1, seed) % 24)
        rect(img, x, y, 5, 2, BONE)
        put(img, x + 1, y - 1, BONE_LT)
        put(img, x + 4, y + 1, BONE)
        if i & 1:
            # tiny skull
            rect(img, x, y, 4, 4, BONE)
            put(img, x + 1, y + 1, INK)
            put(img, x + 2, y + 1, INK)
            put(img, x + 1, y + 3, INK)
            put(img, x + 2, y + 3, INK)
    return img


def tile_cosmic_stone(seed: int) -> np.ndarray:
    img = blank(STONE_DK)
    texture_fill(img, INK, STONE, seed, 0.5)
    cracks(img, PURPLE, seed, 5)
    cracks(img, VOID_GLOW, seed + 3, 3)
    sprinkle(img, COSMIC, seed + 5, 7)
    sprinkle(img, COSMIC, seed + 8, 3)
    return img


def tile_sand(seed: int) -> np.ndarray:
    img = blank(SAND)
    texture_fill(img, CLAY, BONE, seed, 0.35)
    for y in range(W):
        for x in range(W):
            if (y + (n2(x, 0, seed) & 3)) % 6 == 0 and n2(x, y, seed) > 40:
                img[y, x] = DUST
    sprinkle(img, AMBER, seed, 5)
    return img


def tile_flagstone(seed: int) -> np.ndarray:
    img = blank(STONE)
    texture_fill(img, STONE_DK, STONE_LT, seed, 0.3)
    for y in range(0, W, 16):
        for x in range(W):
            put(img, x, y, INK)
            put(img, x, (y + 15) & 31, CRACK)
    for x in range(0, W, 16):
        ox = 0 if (x // 16) % 2 == 0 else 8
        for y in range(W):
            put(img, x, (y + ox) & 31, INK)
    cracks(img, CRACK, seed, 2)
    sprinkle(img, ASH, seed, 12)
    return img


def tile_mosaic(seed: int) -> np.ndarray:
    img = tile_flagstone(seed)
    cx, cy = 15, 16
    for y in range(W):
        for x in range(W):
            d = math.hypot(x - cx, y - cy)
            if 4 < d < 8:
                img[y, x] = GOLD if (int(d) + x + y) & 1 else AMBER
            if d <= 3:
                img[y, x] = BLOOD if n2(x, y, seed) > 90 else RUST
    put(img, cx, cy, GOLD_LT)
    return img


def tile_black_water(frame: int) -> np.ndarray:
    img = blank(VOID)
    for y in range(W):
        for x in range(W):
            t = math.sin((x * 0.4) + (y * 0.25) + frame * 0.9) * 0.5 + 0.5
            img[y, x] = lerp_c(INK, PURPLE, t * 0.6)
            if ((x + y * 2 + frame * 3) & 15) == 0:
                img[y, x] = VOID_GLOW
    return img


def tile_blood_pool(frame: int) -> np.ndarray:
    img = blank(BLOOD)
    for y in range(W):
        for x in range(W):
            t = math.sin((x * 0.35) + frame * 0.7 + y * 0.2) * 0.5 + 0.5
            img[y, x] = lerp_c(BLOOD, BLOOD_LT, t * 0.5)
            if n2(x, y + frame, 9) < 12:
                img[y, x] = RUST
    # rim
    for i in range(W):
        put(img, i, 0, MANTLE_DK)
        put(img, i, 31, ROBE)
        put(img, 0, i, MANTLE_DK)
        put(img, 31, i, ROBE)
    return img


def tile_void_pit() -> np.ndarray:
    img = blank(VOID)
    cx, cy = 15, 16
    for y in range(W):
        for x in range(W):
            d = math.hypot(x - cx, y - cy)
            if d > 14:
                img[y, x] = STONE_DK
            elif d > 11:
                img[y, x] = INK
            else:
                img[y, x] = VOID
                if n2(x, y, 11) < 10 and d < 9:
                    img[y, x] = COSMIC if n2(x, y, 12) & 1 else VOID_GLOW
    return img


def tile_magma(frame: int) -> np.ndarray:
    img = blank(RUST)
    for y in range(W):
        for x in range(W):
            t = noise(x, y + frame, 21) / 255.0
            if t > 0.62:
                img[y, x] = LANTERN
            elif t > 0.48:
                img[y, x] = MAGMA
            elif t > 0.35:
                img[y, x] = AMBER
            else:
                img[y, x] = lerp_c(INK, RUST, t)
    cracks(img, INK, 22, 3)
    return img


# ---------------------------------------------------------------------------
# Walls / structures
# ---------------------------------------------------------------------------

def tile_wall(variant: int) -> np.ndarray:
    img = blank(STONE_DK)
    # top of wall
    rect(img, 0, 0, 32, 10, STONE)
    texture_fill(img, STONE_DK, STONE_LT, 30 + variant, 0.25)
    rect(img, 0, 0, 32, 10, STONE_LT)
    for x in range(W):
        img[0, x] = BONE_LT if variant == 0 else STONE_LT
        img[9, x] = INK
        img[10, x] = INK
    # face bricks
    for y in range(11, 32, 6):
        ox = 0 if ((y - 11) // 6) % 2 == 0 else 5
        for x in range(-ox, W, 10):
            col = lerp_c(STONE_DK, STONE, (h2(x, y, 40 + variant) & 255) / 400.0 + 0.2)
            rect(img, x, y, 9, 5, col)
            rect(img, x, y, 9, 1, INK)
    if variant == 1:
        stain(img, BLOOD, 4, 16, 22, 8)
    if variant == 2:
        cracks(img, PURPLE, 8, 3)
        sprinkle(img, VOID_GLOW, 9, 10)
    if variant == 3:
        # sun carving
        for a in range(8):
            ang = a * math.pi / 4
            put(img, int(16 + math.cos(ang) * 5), int(6 + math.sin(ang) * 3), GOLD)
        rect(img, 14, 4, 4, 4, GOLD)
    return img


def tile_wall_top() -> np.ndarray:
    img = blank(STONE)
    texture_fill(img, STONE_DK, STONE_LT, 50, 0.3)
    for y in range(0, W, 8):
        for x in range(W):
            put(img, x, y, INK)
    for x in range(0, W, 8):
        for y in range(W):
            put(img, x, y, INK)
    sprinkle(img, DUST, 3, 14)
    return img


def tile_pillar() -> np.ndarray:
    img = blank(NONE)
    rect(img, 10, 4, 12, 26, STONE)
    rect(img, 9, 4, 14, 4, STONE_LT)
    rect(img, 9, 26, 14, 4, STONE_DK)
    rect(img, 11, 8, 10, 16, STONE_DK)
    put(img, 15, 12, GOLD)
    put(img, 16, 12, GOLD)
    outline_nonzero(img, INK)
    # shadow
    for x in range(10, 22):
        put(img, x, 30, CRACK)
    return img


def tile_pillar_broken() -> np.ndarray:
    img = blank(NONE)
    rect(img, 10, 16, 12, 14, STONE)
    rect(img, 9, 16, 14, 3, STONE_LT)
    rect(img, 11, 28, 10, 3, STONE_DK)
    rect(img, 8, 20, 3, 2, STONE_LT)  # rubble
    rect(img, 22, 24, 4, 3, STONE)
    outline_nonzero(img, INK)
    return img


def tile_altar() -> np.ndarray:
    img = blank(NONE)
    rect(img, 4, 14, 24, 14, STONE)
    rect(img, 6, 12, 20, 6, STONE_LT)
    rect(img, 8, 10, 16, 4, BONE)
    # dying sun disk
    cx, cy = 15, 16
    for y in range(8, 24):
        for x in range(8, 24):
            d = math.hypot(x - cx, y - cy)
            if d < 5:
                img[y, x] = AMBER if d > 2 else LANTERN
            elif d < 6.2:
                img[y, x] = GOLD
    for a in range(8):
        ang = a * math.pi / 4
        put(img, int(cx + math.cos(ang) * 7), int(cy + math.sin(ang) * 5), GOLD)
    stain(img, BLOOD, 2, 16, 24, 6)
    outline_nonzero(img, INK)
    return img


def tile_doorway() -> np.ndarray:
    img = tile_wall(0)
    rect(img, 8, 10, 16, 22, VOID)
    rect(img, 7, 10, 18, 2, STONE_LT)
    rect(img, 7, 10, 2, 22, STONE)
    rect(img, 23, 10, 2, 22, STONE)
    sprinkle(img, PURPLE, 6, 20)
    return img


def tile_stairs() -> np.ndarray:
    img = blank(STONE_DK)
    for i in range(6):
        y = 2 + i * 5
        col = lerp_c(STONE_DK, STONE_LT, i / 6.0)
        rect(img, 4 + i, y, 24 - i * 2, 5, col)
        for x in range(4 + i, 28 - i):
            put(img, x, y, INK)
    return img


def tile_well() -> np.ndarray:
    img = blank(NONE)
    cx, cy = 15, 16
    for y in range(W):
        for x in range(W):
            d = math.hypot(x - cx, y - cy)
            if 8 < d < 12:
                img[y, x] = STONE_LT if y < cy else STONE_DK
            elif d <= 8:
                img[y, x] = VOID
                if d < 5 and n2(x, y, 1) < 40:
                    img[y, x] = PURPLE
    outline_nonzero(img, INK)
    return img


def tile_sarcophagus() -> np.ndarray:
    img = blank(NONE)
    rect(img, 6, 8, 20, 20, STONE)
    rect(img, 7, 9, 18, 16, STONE_LT)
    rect(img, 10, 12, 12, 10, BONE)
    put(img, 14, 15, INK)
    put(img, 17, 15, INK)
    rect(img, 13, 18, 6, 2, INK)
    stain(img, GOLD, 1, 16, 14, 4)
    outline_nonzero(img, INK)
    return img


def tile_obelisk() -> np.ndarray:
    img = blank(NONE)
    for i, w in enumerate((4, 6, 8, 8, 8, 8, 8, 8, 8, 10, 12)):
        x = 16 - w // 2
        y = 4 + i * 2
        rect(img, x, y, w, 2, STONE_LT if i < 2 else STONE)
    put(img, 15, 10, GOLD)
    put(img, 16, 10, GOLD)
    put(img, 15, 11, AMBER)
    put(img, 16, 11, AMBER)
    outline_nonzero(img, INK)
    return img


def tile_statue() -> np.ndarray:
    img = blank(NONE)
    rect(img, 12, 20, 8, 8, STONE)  # knees
    rect(img, 11, 12, 10, 10, STONE_LT)  # body
    rect(img, 12, 6, 8, 8, STONE)  # hood
    rect(img, 13, 8, 6, 5, CRACK)
    put(img, 15, 10, INK)
    put(img, 16, 10, INK)
    outline_nonzero(img, INK)
    return img


def tile_tentacle() -> np.ndarray:
    img = blank(NONE)
    pts = [(16, 30), (14, 24), (18, 18), (12, 12), (20, 8), (15, 4)]
    for i, (x, y) in enumerate(pts):
        r = 5 - i // 2
        for yy in range(y - r, y + r + 1):
            for xx in range(x - r, x + r + 1):
                if math.hypot(xx - x, yy - y) <= r:
                    put(img, xx, yy, FLESH if i % 2 == 0 else PURPLE)
        put(img, x, y, COSMIC)
    outline_nonzero(img, INK)
    return img


def tile_eye_stone() -> np.ndarray:
    img = tile_flagstone(4)
    rect(img, 10, 10, 12, 12, INK)
    rect(img, 12, 12, 8, 8, EYE)
    rect(img, 14, 13, 4, 6, PURPLE)
    rect(img, 15, 14, 2, 4, INK)
    put(img, 15, 16, COSMIC)
    put(img, 16, 16, COSMIC)
    return img


def tile_rubble() -> np.ndarray:
    img = tile_ash(12)
    rect(img, 4, 18, 10, 6, STONE)
    rect(img, 16, 14, 8, 8, STONE_LT)
    rect(img, 20, 22, 7, 5, STONE_DK)
    outline_nonzero(img, INK)
    return img


def tile_grate() -> np.ndarray:
    img = blank(INK)
    texture_fill(img, VOID, STONE_DK, 60, 0.2)
    for i in range(2, 30, 4):
        rect(img, i, 2, 2, 28, STONE)
        rect(img, 2, i, 28, 2, STONE_DK)
    rect(img, 1, 1, 30, 2, STONE_LT)
    rect(img, 1, 29, 30, 2, STONE_DK)
    return img


# ---------------------------------------------------------------------------
# Props (transparent)
# ---------------------------------------------------------------------------

def prop_skull() -> np.ndarray:
    img = blank(NONE)
    rect(img, 11, 10, 10, 10, BONE)
    rect(img, 12, 11, 8, 6, BONE_LT)
    put(img, 14, 13, INK)
    put(img, 15, 13, INK)
    put(img, 17, 13, INK)
    put(img, 18, 13, INK)
    rect(img, 14, 16, 4, 2, INK)
    rect(img, 13, 19, 8, 3, BONE)
    outline_nonzero(img, INK)
    return img


def prop_bones() -> np.ndarray:
    img = blank(NONE)
    rect(img, 6, 18, 14, 2, BONE)
    rect(img, 18, 14, 2, 10, BONE)
    rect(img, 10, 12, 10, 2, BONE_LT)
    put(img, 8, 17, BONE_LT)
    put(img, 22, 20, BONE)
    outline_nonzero(img, INK)
    return img


def prop_lantern() -> np.ndarray:
    img = blank(NONE)
    rect(img, 13, 8, 6, 2, STONE_LT)
    rect(img, 14, 6, 4, 3, STONE)
    rect(img, 13, 10, 6, 10, AMBER)
    rect(img, 14, 11, 4, 7, LANTERN)
    put(img, 15, 14, GOLD_LT)
    put(img, 16, 14, GOLD_LT)
    rect(img, 13, 20, 6, 2, STONE)
    rect(img, 14, 22, 4, 4, STONE_DK)
    # glow
    for y, x in ((12, 12), (12, 19), (18, 12), (18, 19), (10, 15), (16, 10)):
        put(img, x, y, AMBER)
    outline_nonzero(img, INK)
    return img


def prop_candles() -> np.ndarray:
    img = blank(NONE)
    for i, (x, h) in enumerate(((11, 8), (16, 11), (21, 7))):
        rect(img, x, 24 - h, 3, h, BONE)
        put(img, x + 1, 23 - h, LANTERN)
        put(img, x + 1, 22 - h, AMBER)
        put(img, x, 24, STONE_DK)
    outline_nonzero(img, INK)
    return img


def prop_tome() -> np.ndarray:
    img = blank(NONE)
    rect(img, 8, 12, 16, 12, ROBE)
    rect(img, 9, 13, 14, 10, MANTLE)
    rect(img, 12, 16, 8, 4, GOLD)
    put(img, 15, 17, AMBER)
    put(img, 16, 18, AMBER)
    outline_nonzero(img, INK)
    return img


def prop_chest() -> np.ndarray:
    img = blank(NONE)
    rect(img, 6, 14, 20, 12, ROBE)
    rect(img, 6, 14, 20, 5, CLAY)
    rect(img, 6, 18, 20, 2, GOLD)
    put(img, 15, 19, GOLD_LT)
    put(img, 16, 19, GOLD_LT)
    outline_nonzero(img, INK)
    return img


def prop_banner() -> np.ndarray:
    img = blank(NONE)
    rect(img, 15, 4, 2, 26, STONE)
    rect(img, 8, 6, 16, 14, MANTLE)
    rect(img, 9, 7, 14, 12, MANTLE_DK)
    # sun
    rect(img, 13, 10, 6, 6, GOLD)
    put(img, 12, 12, GOLD)
    put(img, 19, 12, GOLD)
    outline_nonzero(img, INK)
    return img


def prop_crystal() -> np.ndarray:
    img = blank(NONE)
    pts = [(16, 6), (10, 22), (22, 22)]
    for y in range(6, 24):
        t = (y - 6) / 18.0
        w = int(2 + t * 6)
        for x in range(16 - w, 16 + w):
            put(img, x, y, COSMIC if (x + y) & 1 else VOID_GLOW)
    put(img, 16, 8, EYE)
    outline_nonzero(img, INK)
    return img


def prop_dead_tree() -> np.ndarray:
    img = blank(NONE)
    rect(img, 14, 14, 4, 16, ROBE)
    rect(img, 8, 10, 16, 3, ROBE)
    rect(img, 6, 6, 3, 8, HOOD)
    rect(img, 20, 8, 3, 10, HOOD)
    rect(img, 16, 4, 3, 8, HOOD)
    put(img, 10, 5, ASH)
    outline_nonzero(img, INK)
    return img


def prop_mushroom() -> np.ndarray:
    img = blank(NONE)
    rect(img, 14, 18, 4, 10, BONE)
    rect(img, 8, 10, 16, 10, PURPLE)
    rect(img, 10, 8, 12, 6, VOID_GLOW)
    sprinkle(img, COSMIC, 3, 40)
    outline_nonzero(img, INK)
    return img


def prop_sigil() -> np.ndarray:
    img = blank(NONE)
    cx, cy = 15, 16
    for y in range(W):
        for x in range(W):
            d = abs(math.hypot(x - cx, y - cy) - 10)
            if d < 1.1:
                img[y, x] = VOID_GLOW
            d2 = abs(math.hypot(x - cx, y - cy) - 5)
            if d2 < 0.9:
                img[y, x] = BLOOD_LT
    for a in range(6):
        ang = a * math.pi / 3
        for r in range(0, 10):
            put(img, int(cx + math.cos(ang) * r), int(cy + math.sin(ang) * r), COSMIC)
    return img


def prop_key() -> np.ndarray:
    img = blank(NONE)
    rect(img, 10, 12, 8, 8, GOLD)
    rect(img, 12, 14, 4, 4, NONE)
    rect(img, 16, 15, 10, 2, GOLD)
    put(img, 22, 17, GOLD)
    put(img, 24, 17, GOLD)
    outline_nonzero(img, INK)
    return img


def prop_sword() -> np.ndarray:
    img = blank(NONE)
    rect(img, 15, 6, 2, 16, STONE_LT)
    rect(img, 15, 6, 2, 3, EYE)
    rect(img, 12, 20, 8, 3, GOLD)
    rect(img, 14, 22, 4, 6, ROBE)
    stain(img, BLOOD, 1, 16, 10, 3)
    outline_nonzero(img, INK)
    return img


def prop_cairn() -> np.ndarray:
    img = blank(NONE)
    rect(img, 10, 22, 12, 6, STONE)
    rect(img, 12, 16, 8, 7, STONE_LT)
    rect(img, 14, 11, 6, 6, STONE)
    outline_nonzero(img, INK)
    return img


def prop_brazier() -> np.ndarray:
    img = blank(NONE)
    rect(img, 10, 20, 12, 6, STONE)
    rect(img, 12, 18, 8, 3, STONE_LT)
    rect(img, 13, 10, 6, 9, AMBER)
    rect(img, 14, 8, 4, 6, LANTERN)
    put(img, 15, 7, GOLD_LT)
    put(img, 16, 6, LANTERN)
    outline_nonzero(img, INK)
    return img


def prop_egg() -> np.ndarray:
    img = blank(NONE)
    for y in range(8, 28):
        t = (y - 8) / 20.0
        w = int(3 + math.sin(t * math.pi) * 7)
        for x in range(16 - w, 16 + w):
            put(img, x, y, PURPLE if (x + y) % 3 else FLESH)
    put(img, 15, 16, COSMIC)
    put(img, 16, 17, COSMIC)
    outline_nonzero(img, INK)
    return img


def prop_raven() -> np.ndarray:
    img = blank(NONE)
    rect(img, 8, 16, 16, 4, INK)
    rect(img, 6, 17, 6, 3, HOOD)
    put(img, 5, 18, HOOD)
    rect(img, 20, 14, 8, 3, INK)
    put(img, 26, 13, INK)
    put(img, 10, 15, INK)
    outline_nonzero(img, CRACK)
    return img


# ---------------------------------------------------------------------------
# Player — 4 dir x 4 walk frames
# ---------------------------------------------------------------------------

def _shadow(img: np.ndarray) -> None:
    for x in range(11, 21):
        put(img, x, 30, CRACK)
    for x in range(12, 20):
        put(img, x, 29, INK)


def _lantern(img: np.ndarray, x: int, y: int) -> None:
    put(img, x, y, STONE_LT)
    put(img, x + 1, y, STONE)
    put(img, x, y + 1, AMBER)
    put(img, x + 1, y + 1, LANTERN)
    put(img, x, y + 2, AMBER)
    put(img, x + 1, y + 2, GOLD)
    put(img, x - 1, y + 1, AMBER)
    put(img, x + 2, y + 1, AMBER)


def player_frame(direction: str, frame: int) -> np.ndarray:
    img = blank(NONE)
    bob = (0, -1, 0, 0)[frame]
    step = (0, 1, 0, -1)[frame]
    y0 = 4 + bob

    _shadow(img)

    if direction == "down":
        # hood
        rect(img, 12, y0 + 2, 8, 7, HOOD)
        rect(img, 11, y0 + 3, 10, 6, HOOD)
        rect(img, 13, y0 + 4, 6, 5, ROBE)
        # mask + eye
        rect(img, 14, y0 + 5, 4, 4, BONE)
        put(img, 15, y0 + 6, EYE)
        put(img, 16, y0 + 6, EYE)
        put(img, 15, y0 + 7, INK)
        put(img, 16, y0 + 7, INK)
        # mantle
        rect(img, 10, y0 + 9, 12, 5, MANTLE)
        rect(img, 11, y0 + 9, 10, 3, MANTLE_DK)
        # amulet
        rect(img, 14, y0 + 10, 4, 4, GOLD)
        put(img, 15, y0 + 11, AMBER)
        put(img, 16, y0 + 11, LANTERN)
        # robe body
        rect(img, 11, y0 + 13, 10, 10, ROBE)
        rect(img, 12, y0 + 14, 8, 8, HOOD)
        # lantern left
        _lantern(img, 8, y0 + 14)
        # feet
        lx = 12 + (step if step > 0 else 0)
        rx = 17 + (0 if step > 0 else -step)
        rect(img, lx, y0 + 22, 3, 3, CRACK)
        rect(img, rx, y0 + 22, 3, 3, CRACK)
        put(img, lx, y0 + 23, HOOD)
        put(img, rx, y0 + 23, HOOD)

    elif direction == "up":
        rect(img, 11, y0 + 2, 10, 8, HOOD)
        rect(img, 12, y0 + 1, 8, 4, HOOD)
        rect(img, 10, y0 + 9, 12, 5, MANTLE)
        rect(img, 11, y0 + 10, 10, 4, MANTLE_DK)
        rect(img, 11, y0 + 13, 10, 10, ROBE)
        rect(img, 12, y0 + 14, 8, 8, HOOD)
        _lantern(img, 21, y0 + 14)
        lx = 12 + (0 if step > 0 else -step)
        rx = 17 + (step if step > 0 else 0)
        rect(img, lx, y0 + 22, 3, 3, CRACK)
        rect(img, rx, y0 + 22, 3, 3, CRACK)

    elif direction == "left":
        rect(img, 12, y0 + 2, 8, 8, HOOD)
        rect(img, 11, y0 + 3, 8, 6, HOOD)
        rect(img, 13, y0 + 5, 4, 4, BONE)
        put(img, 13, y0 + 6, EYE)
        put(img, 13, y0 + 7, INK)
        rect(img, 11, y0 + 9, 10, 5, MANTLE)
        rect(img, 12, y0 + 13, 8, 10, ROBE)
        rect(img, 13, y0 + 14, 6, 8, HOOD)
        _lantern(img, 8, y0 + 13)
        fx = 14 + step
        rect(img, fx, y0 + 22, 3, 3, CRACK)
        rect(img, fx + 3, y0 + 22 - abs(step), 3, 3, CRACK)

    else:  # right
        rect(img, 12, y0 + 2, 8, 8, HOOD)
        rect(img, 13, y0 + 3, 8, 6, HOOD)
        rect(img, 15, y0 + 5, 4, 4, BONE)
        put(img, 18, y0 + 6, EYE)
        put(img, 18, y0 + 7, INK)
        rect(img, 11, y0 + 9, 10, 5, MANTLE)
        rect(img, 12, y0 + 13, 8, 10, ROBE)
        rect(img, 13, y0 + 14, 6, 8, HOOD)
        _lantern(img, 22, y0 + 13)
        fx = 14 - step
        rect(img, fx, y0 + 22, 3, 3, CRACK)
        rect(img, fx + 3, y0 + 22 - abs(step), 3, 3, CRACK)

    outline_nonzero(img, INK)
    return img


# ---------------------------------------------------------------------------
# Quantize / extract
# ---------------------------------------------------------------------------

def nearest_palette(px):
    r, g, b = int(px[0]), int(px[1]), int(px[2])
    best = PALETTE_RGB[0]
    bd = 1e9
    for p in PALETTE_RGB:
        d = (r - p[0]) ** 2 + (g - p[1]) ** 2 + (b - p[2]) ** 2
        if d < bd:
            bd = d
            best = p
    return best + (255,)


def quantize_img(arr: np.ndarray) -> np.ndarray:
    out = arr.copy()
    h, w = arr.shape[:2]
    for y in range(h):
        for x in range(w):
            if arr[y, x, 3] < 16:
                out[y, x] = (0, 0, 0, 0)
            else:
                c = nearest_palette(arr[y, x])
                out[y, x] = c
    return out


def key_magenta(arr: np.ndarray) -> np.ndarray:
    out = arr.copy()
    if out.shape[2] == 3:
        a = np.full((*out.shape[:2], 1), 255, dtype=np.uint8)
        out = np.concatenate([out, a], axis=2)
    r, g, b = out[:, :, 0], out[:, :, 1], out[:, :, 2]
    mag = (r > 180) & (g < 90) & (b > 180)
    # also near-white-magenta / hot pink
    mag |= (r > 200) & (b > 180) & (g < 140)
    out[mag, 3] = 0
    return out


def extract_grid(path: Path, cols: int, rows: int, inset: int) -> list[np.ndarray]:
    im = Image.open(path).convert("RGB")
    arr = np.array(im)
    h, w = arr.shape[:2]
    cw, ch = w // cols, h // rows
    tiles = []
    for r in range(rows):
        for c in range(cols):
            x0 = c * cw + inset
            y0 = r * ch + inset
            x1 = (c + 1) * cw - inset
            y1 = (r + 1) * ch - inset
            cell = Image.fromarray(arr[y0:y1, x0:x1]).resize(
                (W, W), Image.Resampling.BOX
            )
            rgba = np.array(cell.convert("RGBA"))
            tiles.append(quantize_img(rgba))
    return tiles


def extract_player_ai() -> list[np.ndarray] | None:
    path = SRC / "dsp_player_sheet.png"
    if not path.exists():
        return None
    im = Image.open(path).convert("RGB")
    arr = np.array(im)
    mag = (arr[:, :, 0] > 200) & (arr[:, :, 1] < 80) & (arr[:, :, 2] > 200)
    content = ~mag
    row = content.mean(axis=1)
    col = content.mean(axis=0)

    def runs(v, thresh=0.08):
        out = []
        inside = False
        start = 0
        for i, x in enumerate(v):
            if x > thresh and not inside:
                inside, start = True, i
            elif x <= thresh and inside:
                inside = False
                out.append((start, i))
        if inside:
            out.append((start, len(v)))
        return out

    rr, cc = runs(row), runs(col)
    if len(rr) < 4 or len(cc) < 4:
        return None
    rr, cc = rr[:4], cc[:4]
    frames = []
    for r0, r1 in rr:
        for c0, c1 in cc:
            cell = arr[r0:r1, c0:c1]
            rgba = key_magenta(np.array(Image.fromarray(cell).convert("RGBA")))
            # crop to opaque
            a = rgba[:, :, 3] > 16
            ys, xs = np.where(a)
            if len(ys) == 0:
                frames.append(blank())
                continue
            crop = rgba[ys.min() : ys.max() + 1, xs.min() : xs.max() + 1]
            ch, cw = crop.shape[:2]
            side = max(ch, cw) + 2
            pad = np.zeros((side, side, 4), dtype=np.uint8)
            ox = (side - cw) // 2
            oy = side - ch - 1  # feet to bottom
            pad[oy : oy + ch, ox : ox + cw] = crop
            small = Image.fromarray(pad).resize((W, W), Image.Resampling.BOX)
            frames.append(quantize_img(np.array(small)))
    return frames


def extract_prop_blobs(path: Path, limit: int = 40) -> list[np.ndarray]:
    im = Image.open(path).convert("RGB")
    arr = np.array(im)
    mag = (arr[:, :, 0] > 200) & (arr[:, :, 1] < 90) & (arr[:, :, 2] > 180)
    content = (~mag).astype(np.uint8)
    h, w = content.shape
    visited = np.zeros_like(content)
    blobs = []
    for y in range(0, h, 2):
        for x in range(0, w, 2):
            if content[y, x] == 0 or visited[y, x]:
                continue
            stack = [(x, y)]
            visited[y, x] = 1
            minx = maxx = x
            miny = maxy = y
            n = 0
            while stack:
                cx, cy = stack.pop()
                n += 1
                minx, maxx = min(minx, cx), max(maxx, cx)
                miny, maxy = min(miny, cy), max(maxy, cy)
                for dx, dy in ((-2, 0), (2, 0), (0, -2), (0, 2)):
                    nx, ny = cx + dx, cy + dy
                    if 0 <= nx < w and 0 <= ny < h and content[ny, nx] and not visited[ny, nx]:
                        visited[ny, nx] = 1
                        stack.append((nx, ny))
            if n < 80 or (maxx - minx) < 20 or (maxy - miny) < 20:
                continue
            pad = 4
            crop = arr[
                max(0, miny - pad) : min(h, maxy + pad),
                max(0, minx - pad) : min(w, maxx + pad),
            ]
            blobs.append((n, crop))
    blobs.sort(key=lambda t: -t[0])
    tiles = []
    for _, crop in blobs[:limit]:
        rgba = key_magenta(np.array(Image.fromarray(crop).convert("RGBA")))
        a = rgba[:, :, 3] > 16
        ys, xs = np.where(a)
        if len(ys) == 0:
            continue
        crop = rgba[ys.min() : ys.max() + 1, xs.min() : xs.max() + 1]
        ch, cw = crop.shape[:2]
        side = max(ch, cw, 8)
        pad = np.zeros((side, side, 4), dtype=np.uint8)
        ox = (side - cw) // 2
        oy = (side - ch) // 2
        pad[oy : oy + ch, ox : ox + cw] = crop
        small = Image.fromarray(pad).resize((W, W), Image.Resampling.BOX)
        tiles.append(quantize_img(np.array(small)))
    return tiles


def to_pil(img: np.ndarray) -> Image.Image:
    return Image.fromarray(img, "RGBA")


def sheet_from(tiles: list[np.ndarray], columns: int) -> Image.Image:
    n = len(tiles)
    rows = (n + columns - 1) // columns
    sheet = Image.new("RGBA", (columns * W, rows * W), (0, 0, 0, 0))
    for i, t in enumerate(tiles):
        c, r = i % columns, i // columns
        sheet.paste(to_pil(t), (c * W, r * W))
    return sheet


def opaque(img: np.ndarray, fallback=VOID) -> np.ndarray:
    out = img.copy()
    mask = out[:, :, 3] < 16
    out[mask] = fallback
    out[:, :, 3] = 255
    return out


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)

    # --- player ---
    dirs = ["down", "left", "right", "up"]
    player_tiles = [player_frame(d, f) for d in dirs for f in range(4)]
    player_ai = extract_player_ai()
    player_sheet = sheet_from(player_tiles, 4)
    player_sheet.save(OUT / "player.png")
    if player_ai and len(player_ai) == 16:
        sheet_from(player_ai, 4).save(OUT / "player_painted.png")

    # --- tileset (16x16), GID = index + 1 ---
    names: list[str] = []
    tiles: list[np.ndarray] = []

    def add(name: str, tile: np.ndarray, make_opaque: bool = True) -> None:
        names.append(name)
        tiles.append(opaque(tile) if make_opaque else tile)

    # 1-8 ash / earth
    for i in range(4):
        add(f"ash_{i}", tile_ash(10 + i * 17))
    for i in range(4):
        add(f"earth_{i}", tile_cracked_earth(20 + i * 13))
    # 9-16 paths / ritual
    for i in range(3):
        add(f"cobble_{i}", tile_cobble(30 + i * 9))
    add("ritual", tile_ritual(1))
    add("flagstone_0", tile_flagstone(1))
    add("flagstone_1", tile_flagstone(2))
    add("mosaic", tile_mosaic(3))
    add("sand_0", tile_sand(1))
    # 17-24 horror floors
    for i in range(2):
        add(f"blood_{i}", tile_blood_dirt(40 + i * 11))
    for i in range(2):
        add(f"bone_floor_{i}", tile_bone_ground(50 + i * 7))
    for i in range(2):
        add(f"cosmic_{i}", tile_cosmic_stone(60 + i * 5))
    add("void_grass_0", tile_void_grass(1))
    add("void_grass_1", tile_void_grass(2))
    # 25-32 liquids / hazards (anim: water 25-28, magma 29-32)
    for f in range(4):
        add(f"water_{f}", tile_black_water(f))
    for f in range(4):
        add(f"magma_{f}", tile_magma(f))
    # 33-40 pits / pools
    add("void_pit", tile_void_pit())
    for f in range(3):
        add(f"blood_pool_{f}", tile_blood_pool(f))
    add("sand_1", tile_sand(8))
    add("ash_4", tile_ash(99))
    add("cobble_3", tile_cobble(77))
    add("earth_4", tile_cracked_earth(88))
    # 41-48 walls
    for v in range(4):
        add(f"wall_{v}", tile_wall(v))
    add("wall_top", tile_wall_top())
    add("doorway", tile_doorway())
    add("stairs", tile_stairs())
    add("rubble", tile_rubble())
    # 49-56 structures
    add("pillar", tile_pillar())
    add("pillar_broken", tile_pillar_broken())
    add("altar", tile_altar())
    add("well", tile_well())
    add("sarcophagus", tile_sarcophagus())
    add("obelisk", tile_obelisk())
    add("statue", tile_statue())
    add("grate", tile_grate())
    # 57-64 more structures
    add("tentacle", tile_tentacle())
    add("eye_stone", tile_eye_stone())
    add("wall_blood", tile_wall(1))
    add("wall_void", tile_wall(2))
    add("wall_sun", tile_wall(3))
    add("pillar2", tile_pillar())
    add("altar_copy", tile_altar())
    add("rubble_2", tile_rubble())

    # remaining: extracted painted tiles from AI ground/structures
    painted: list[np.ndarray] = []
    gp = SRC / "dsp_tileset_ground.png"
    sp = SRC / "dsp_tileset_structures.png"
    if gp.exists():
        painted.extend(extract_grid(gp, 8, 8, 5))
    if sp.exists():
        painted.extend(extract_grid(sp, 8, 8, 5))
    for i, t in enumerate(painted):
        if len(tiles) >= COLS * ROWS:
            break
        add(f"painted_{i}", t)

    while len(tiles) < COLS * ROWS:
        add(f"fill_{len(tiles)}", tile_ash(200 + len(tiles)))

    tiles = tiles[: COLS * ROWS]
    names = names[: COLS * ROWS]
    tileset = sheet_from(tiles, COLS)
    tileset.save(OUT / "tileset.png")

    # --- props sheet 8x8 ---
    props = [
        ("skull", prop_skull()),
        ("bones", prop_bones()),
        ("lantern", prop_lantern()),
        ("candles", prop_candles()),
        ("tome", prop_tome()),
        ("chest", prop_chest()),
        ("banner", prop_banner()),
        ("crystal", prop_crystal()),
        ("dead_tree", prop_dead_tree()),
        ("mushroom", prop_mushroom()),
        ("sigil", prop_sigil()),
        ("key", prop_key()),
        ("sword", prop_sword()),
        ("cairn", prop_cairn()),
        ("brazier", prop_brazier()),
        ("egg", prop_egg()),
        ("raven", prop_raven()),
        ("tentacle_prop", tile_tentacle()),
        ("statue_prop", tile_statue()),
        ("pillar_prop", tile_pillar()),
        ("altar_prop", tile_altar()),
        ("obelisk_prop", tile_obelisk()),
        ("well_prop", tile_well()),
        ("sarcophagus_prop", tile_sarcophagus()),
    ]
    pp = SRC / "dsp_tileset_props.png"
    if pp.exists():
        for i, t in enumerate(extract_prop_blobs(pp, 40)):
            props.append((f"painted_prop_{i}", t))
    while len(props) < 64:
        props.append((f"empty_{len(props)}", blank()))
    props = props[:64]
    sheet_from([p[1] for p in props], 8).save(OUT / "props.png")

    # --- sample map (Tiled GID = tile index + 1) ---
    # tiles: ash_0=1 ... cobble_0=9, ritual=12, mosaic=15,
    # wall_0=41, doorway=46, stairs=47, pillar=49, altar=51, well=52,
    # sarcophagus=53, obelisk=54, statue=55, tentacle=57, eye_stone=58
    ASH, COB, RIT, MOS = 1, 9, 12, 15
    EARTH, BLOODT, BONEF, COS = 5, 17, 19, 21
    WATER, MAGMA, PIT = 25, 29, 33
    WALL, DOOR, STAIR = 41, 46, 47
    PILLAR, ALTAR, WELL = 49, 51, 52
    SARCO, OBELISK, STATUE = 53, 54, 55
    TENT, EYE = 57, 58
    GRATE = 56

    mw, mh = 40, 28
    grid = [[ASH + ((x * 3 + y * 7) % 4) for x in range(mw)] for y in range(mh)]

    def set_t(x, y, t):
        if 0 <= x < mw and 0 <= y < mh:
            grid[y][x] = t

    # processional cobble road down the center
    for y in range(mh):
        for x in range(18, 22):
            set_t(x, y, COB + (x + y) % 3)
    # ritual plaza north
    for y in range(2, 9):
        for x in range(14, 26):
            set_t(x, y, 13 if (x + y) % 2 == 0 else 14)
    set_t(19, 4, MOS)
    set_t(20, 4, MOS)
    set_t(19, 5, ALTAR)
    set_t(20, 5, ALTAR)
    set_t(17, 3, PILLAR)
    set_t(22, 3, PILLAR)
    set_t(16, 6, OBELISK)
    set_t(23, 6, STATUE)
    set_t(15, 7, WALL)
    set_t(16, 7, WALL)
    set_t(17, 7, WALL)
    set_t(22, 7, WALL)
    set_t(23, 7, WALL)
    set_t(24, 7, WALL)
    set_t(19, 8, STAIR)
    set_t(20, 8, STAIR)

    # ruined chapel west
    for y in range(12, 20):
        for x in range(4, 12):
            if x in (4, 11) or y in (12, 19):
                set_t(x, y, WALL + (x + y) % 4)
            else:
                set_t(x, y, 13)
    set_t(7, 12, DOOR)
    set_t(8, 12, DOOR)
    set_t(7, 16, SARCO)
    set_t(8, 15, WELL)
    set_t(6, 14, EYE)

    # void pits east
    for y in range(10, 18):
        for x in range(28, 36):
            set_t(x, y, COS + (x + y) % 2)
    set_t(31, 13, PIT)
    set_t(32, 13, PIT)
    set_t(31, 14, PIT)
    set_t(32, 14, PIT)
    set_t(30, 12, TENT)
    set_t(33, 16, TENT)
    for y in range(20, 24):
        for x in range(26, 34):
            set_t(x, y, MAGMA)

    # blood trail on the road
    for y in range(10, 26, 3):
        set_t(19, y, BLOODT)
        set_t(20, y, BLOODT + 1)

    # bone field south
    for y in range(22, 27):
        for x in range(8, 16):
            set_t(x, y, BONEF + (x % 2))

    # water west-south
    for y in range(20, 27):
        for x in range(1, 7):
            set_t(x, y, WATER + ((x + y) % 4))

    set_t(19, 24, GRATE)
    set_t(20, 24, GRATE)

    lines = [",".join(str(v) for v in row) for row in grid]
    (OUT / "map.csv").write_text("\n".join(lines) + "\n", encoding="utf-8")

    solid = set()
    for i, n in enumerate(names):
        if n.startswith(("wall", "doorway", "pillar", "altar", "well", "sarco",
                         "obelisk", "statue", "tentacle", "rubble")):
            solid.add(i + 1)

    atlas = {
        "tile_size": 32,
        "player": {
            "file": "player.png",
            "columns": 4,
            "rows": 4,
            "notes": "Row 0 down, 1 left, 2 right, 3 up. 4-frame walk.",
            "anims": {
                "walk_down": {"start": 0, "count": 4, "fps": 8},
                "walk_left": {"start": 4, "count": 4, "fps": 8},
                "walk_right": {"start": 8, "count": 4, "fps": 8},
                "walk_up": {"start": 12, "count": 4, "fps": 8},
            },
        },
        "tileset": {
            "file": "tileset.png",
            "columns": COLS,
            "rows": ROWS,
            "empty_id": 0,
            "gid_offset": 1,
            "anims": {
                "water": {"first_id": 25, "frames": 4, "fps": 6},
                "magma": {"first_id": 29, "frames": 4, "fps": 8},
                "blood_pool": {"first_id": 34, "frames": 3, "fps": 4},
            },
            "tiles": {
                str(i + 1): {"name": names[i], "solid": (i + 1) in solid}
                for i in range(len(names))
            },
        },
        "props": {
            "file": "props.png",
            "columns": 8,
            "rows": 8,
            "names": [p[0] for p in props],
        },
        "map": {"file": "map.csv", "width": mw, "height": mh},
    }
    (OUT / "atlas.json").write_text(json.dumps(atlas, indent=2), encoding="utf-8")

    # 4x previews for inspection
    player_sheet.resize((512, 512), Image.Resampling.NEAREST).save(
        OUT / "_preview_player.png"
    )
    tileset.resize((1024, 1024), Image.Resampling.NEAREST).save(
        OUT / "_preview_tileset.png"
    )
    print("wrote", len(tiles), "tiles,", len(props), "props to", OUT)


if __name__ == "__main__":
    main()
