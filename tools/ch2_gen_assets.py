"""Generates the procedural textures and sounds used by the Chapter 2 highway / police post.

Run from the repo root:  py tools/ch2_gen_assets.py
Outputs go to assets/sprites/3d and assets/audio/sfx (deterministic, seeded).
"""
import math
import random
import struct
import wave
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
TEX = ROOT / "assets" / "sprites" / "3d"
SFX = ROOT / "assets" / "audio" / "sfx"
FONT = ROOT / "assets" / "fonts" / "font_main.ttf"


def clamp8(v):
    return max(0, min(255, int(v)))


def noise_tex(name, base, spread, rng, size=64, post=None):
    im = Image.new("RGBA", (size, size))
    px = im.load()
    for y in range(size):
        for x in range(size):
            n = rng.uniform(-spread, spread)
            px[x, y] = (clamp8(base[0] + n), clamp8(base[1] + n * 1.05), clamp8(base[2] + n * 0.9), 255)
    if post:
        post(im, px, rng)
    im.save(TEX / f"{name}.png")


def grass_post(im, px, rng):
    for _ in range(420):
        x, y = rng.randrange(64), rng.randrange(64)
        h = rng.randint(2, 5)
        shade = rng.uniform(0.8, 1.5)
        for k in range(h):
            px[x, (y - k) % 64] = (clamp8(38 * shade), clamp8(58 * shade), clamp8(32 * shade), 255)
    for _ in range(60):
        px[rng.randrange(64), rng.randrange(64)] = (62, 52, 40, 255)


def bark_post(im, px, rng):
    for x in range(64):
        streak = rng.uniform(-14, 14)
        for y in range(64):
            r, g, b, a = px[x, y]
            px[x, y] = (clamp8(r + streak), clamp8(g + streak), clamp8(b + streak * 0.8), 255)
    for _ in range(26):
        x, y = rng.randrange(64), rng.randrange(64)
        for k in range(rng.randint(3, 9)):
            px[x, (y + k) % 64] = (24, 18, 14, 255)


def pine_post(im, px, rng):
    for _ in range(300):
        x, y = rng.randrange(64), rng.randrange(64)
        dx = rng.choice((-1, 1))
        shade = rng.uniform(0.7, 1.6)
        for k in range(rng.randint(2, 4)):
            px[(x + k * dx) % 64, (y + k) % 64] = (clamp8(26 * shade), clamp8(52 * shade), clamp8(34 * shade), 255)
    for _ in range(40):
        px[rng.randrange(64), rng.randrange(64)] = (10, 16, 12, 255)


def paint_post(im, px, rng):
    for _ in range(90):
        px[rng.randrange(64), rng.randrange(64)] = (110, 108, 100, 255)


def barrier_tex(rng):
    im = Image.new("RGBA", (64, 64))
    px = im.load()
    for y in range(64):
        for x in range(64):
            red = ((x + y) // 16) % 2 == 0
            n = rng.uniform(-10, 10)
            c = (190 + n, 36 + n * 0.3, 32 + n * 0.3) if red else (214 + n, 210 + n, 200 + n)
            px[x, y] = (clamp8(c[0]), clamp8(c[1]), clamp8(c[2]), 255)
    im.save(TEX / "motel_barrier.png")


def text_board(name, lines, size, bg, fg, border, rng, font_px):
    w, h = size
    scale = 4
    im = Image.new("RGBA", (w * scale, h * scale), bg + (255,))
    d = ImageDraw.Draw(im)
    d.rectangle([2 * scale, 2 * scale, w * scale - 2 * scale - 1, h * scale - 2 * scale - 1], outline=border + (255,), width=2 * scale)
    font = ImageFont.truetype(str(FONT), font_px * scale)
    for i, line in enumerate(lines):
        box = d.textbbox((0, 0), line, font=font)
        tw, th = box[2] - box[0], box[3] - box[1]
        cy = (i + 0.5) * (h * scale) / len(lines)
        d.text(((w * scale - tw) / 2 - box[0], cy - th / 2 - box[1]), line, font=font, fill=fg + (255,))
    im = im.resize((w, h), Image.NEAREST)
    px = im.load()
    for _ in range(w * h // 18):
        x, y = rng.randrange(w), rng.randrange(h)
        r, g, b, a = px[x, y]
        k = rng.uniform(0.72, 0.95)
        px[x, y] = (clamp8(r * k), clamp8(g * k), clamp8(b * k), 255)
    im.save(TEX / f"{name}.png")


def write_wav(path, samples, rate=22050):
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"".join(struct.pack("<h", max(-32767, min(32767, int(s * 32767)))) for s in samples))


class OnePole:
    def __init__(self, a):
        self.a = a
        self.y = 0.0

    def __call__(self, x):
        self.y += self.a * (x - self.y)
        return self.y


def engine_loop(rng, rate=22050):
    # Exactly one second: every partial has an integer number of cycles, so the loop is seamless.
    n = rate
    lp = OnePole(0.08)
    tail = int(rate * 0.12)
    filt = [lp(rng.uniform(-1, 1)) for _ in range(n + tail)]
    for i in range(tail):
        t = i / tail
        filt[i] = filt[i] * t + filt[n + i] * (1.0 - t)
    out = []
    f = 44.0
    for i in range(n):
        t = i / rate
        firing = (math.sin(2 * math.pi * 2 * f * t) * 0.5 + 0.5) ** 6
        s = 0.42 * math.sin(2 * math.pi * f * t) + 0.22 * math.sin(2 * math.pi * 2 * f * t + 0.6)
        s += 0.12 * math.sin(2 * math.pi * 3 * f * t + 1.1) + 0.07 * math.sin(2 * math.pi * 5 * f * t)
        s += 0.35 * firing * math.sin(2 * math.pi * 4 * f * t)
        s += 0.9 * filt[i] * (0.6 + 0.8 * firing)
        out.append(math.tanh(s * 1.3) * 0.62)
    return out


def pistol_shot(rng, rate=22050):
    n = int(rate * 0.9)
    lp = OnePole(0.35)
    lp2 = OnePole(0.05)
    out = []
    for i in range(n):
        t = i / rate
        w = rng.uniform(-1, 1)
        crack = w * math.exp(-t / 0.006)
        body = lp(w) * math.exp(-t / 0.07) * 1.6
        thump = math.sin(2 * math.pi * (95 - 40 * t) * t) * math.exp(-t / 0.05)
        tail = lp2(w) * math.exp(-t / 0.32) * 2.2
        echo = rng.uniform(-1, 1) * 0.09 * math.exp(-(t - 0.16) / 0.18) if t > 0.16 else 0.0
        out.append(math.tanh((crack + body + thump * 0.9 + tail + echo) * 1.4) * 0.95)
    return out


def click(rng, t, at, ring_f, decay):
    if t < at:
        return 0.0
    tt = t - at
    return rng.uniform(-1, 1) * math.exp(-tt / 0.004) + 0.6 * math.sin(2 * math.pi * ring_f * tt) * math.exp(-tt / decay)


def pistol_reload(rng, rate=22050):
    out = []
    for i in range(int(rate * 1.1)):
        t = i / rate
        s = click(rng, t, 0.05, 1800, 0.03) * 0.6
        s += click(rng, t, 0.42, 1300, 0.05) * 0.9
        s += click(rng, t, 0.78, 2400, 0.025) * 0.7
        s += click(rng, t, 0.86, 1600, 0.04) * 0.8
        out.append(max(-1.0, min(1.0, s * 0.7)))
    return out


def main():
    rng = random.Random(1986)
    noise_tex("motel_grass", (30, 38, 27), 9, rng, post=grass_post)
    noise_tex("motel_bark", (52, 41, 33), 8, rng, post=bark_post)
    noise_tex("motel_pine", (18, 32, 22), 6, rng, post=pine_post)
    noise_tex("motel_paint", (178, 174, 160), 12, rng, post=paint_post)
    barrier_tex(rng)
    blue, white, red = (24, 62, 132), (226, 226, 220), (170, 30, 28)
    text_board("motel_board_post1km", ["ПОСТ ДПС", "1 КМ"], (128, 64), blue, white, white, rng, 17)
    text_board("motel_board_post500", ["ПОСТ ДПС", "500 М"], (128, 64), blue, white, white, rng, 17)
    text_board("motel_board_stop", ["СТОП", "КОНТРОЛЬ"], (128, 64), white, red, red, rng, 17)
    text_board("motel_board_m86", ["М-86", "КМ 86"], (128, 64), blue, white, white, rng, 17)
    text_board("motel_dps_sign", ["ДПС"], (128, 32), (232, 236, 240), (20, 46, 120), (20, 46, 120), rng, 20)
    write_wav(SFX / "sfx_car_engine_loop.wav", engine_loop(rng))
    write_wav(SFX / "sfx_pistol_shot.wav", pistol_shot(rng))
    write_wav(SFX / "sfx_pistol_reload.wav", pistol_reload(rng))


if __name__ == "__main__":
    main()
