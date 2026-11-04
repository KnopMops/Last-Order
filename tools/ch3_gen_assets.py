import os
import wave
import math
import struct
import random
from PIL import Image, ImageDraw

TEX_DIR = "assets/sprites/3d/ch3"
SFX_DIR = "assets/audio/sfx"

os.makedirs(TEX_DIR, exist_ok=True)
os.makedirs(SFX_DIR, exist_ok=True)

# ---------------------------------------------------------------------------
# 1. TEXTURE GENERATORS
# ---------------------------------------------------------------------------

def gen_wallpaper(path):
    img = Image.new("RGBA", (128, 128), (62, 54, 46, 255))
    draw = ImageDraw.Draw(img)
    for x in range(128):
        for y in range(128):
            n = (math.sin(x * 0.15) * math.cos(y * 0.15) * 15 +
                 math.sin((x + y) * 0.08) * 10)
            base_g = 52 + int(n)
            base_r = 68 + int(n * 1.1)
            base_b = 48 + int(n * 0.9)
            draw.point((x, y), fill=(max(20, min(230, base_r)),
                                     max(20, min(230, base_g)),
                                     max(20, min(230, base_b)), 255))
    # Pattern
    for bx in range(0, 128, 32):
        for by in range(0, 128, 32):
            draw.ellipse([bx + 10, by + 10, bx + 22, by + 22], outline=(95, 78, 62, 160))
            draw.line([bx + 16, by + 6, bx + 16, by + 26], fill=(95, 78, 62, 140))
            draw.line([bx + 6, by + 16, bx + 26, by + 16], fill=(95, 78, 62, 140))
    # Peeling plaster patches
    random.seed(42)
    for _ in range(8):
        px = random.randint(10, 100)
        py = random.randint(10, 100)
        pr = random.randint(8, 20)
        for dx in range(-pr, pr):
            for dy in range(-pr, pr):
                if dx*dx + dy*dy < pr*pr:
                    nx, ny = px + dx, py + dy
                    if 0 <= nx < 128 and 0 <= ny < 128:
                        draw.point((nx, ny), fill=(135, 128, 115, 255))
    img.save(path)
    print(f"Generated {path}")

def gen_parquet(path):
    img = Image.new("RGBA", (128, 128), (45, 28, 16, 255))
    draw = ImageDraw.Draw(img)
    # Herringbone pattern
    for y in range(0, 128, 16):
        for x in range(0, 128, 32):
            c = 65 + (x * 7 + y * 13) % 25
            r = c + 18
            g = c - 5
            b = c - 28
            draw.rectangle([x, y, x + 30, y + 14], fill=(r, g, b, 255))
            draw.line([x, y, x + 31, y], fill=(22, 14, 8, 255))
            draw.line([x, y, x, y + 15], fill=(22, 14, 8, 255))
    # Wood grain scratches
    for _ in range(120):
        sx = random.randint(0, 125)
        sy = random.randint(0, 125)
        sl = random.randint(4, 18)
        draw.line([sx, sy, sx + sl, sy], fill=(30, 18, 10, 180))
    img.save(path)
    print(f"Generated {path}")

def gen_planks(path):
    img = Image.new("RGBA", (128, 128), (80, 58, 38, 255))
    draw = ImageDraw.Draw(img)
    for i in range(4):
        py = i * 32
        shade = (i % 2) * 12
        draw.rectangle([0, py, 127, py + 30], fill=(85 + shade, 62 + shade, 40 + shade, 255))
        draw.line([0, py + 31, 127, py + 31], fill=(24, 16, 10, 255))
        # Nails
        for nx in [12, 116]:
            draw.ellipse([nx - 2, py + 14, nx + 2, py + 18], fill=(38, 38, 42, 255))
            draw.point((nx, py + 16), fill=(160, 160, 170, 255))
    img.save(path)
    print(f"Generated {path}")

def gen_basement_brick(path):
    img = Image.new("RGBA", (128, 128), (35, 30, 28, 255))
    draw = ImageDraw.Draw(img)
    for row in range(8):
        by = row * 16
        shift = (row % 2) * 16
        for col in range(-1, 5):
            bx = col * 32 + shift
            draw.rectangle([bx + 1, by + 1, bx + 30, by + 14], fill=(72 + (bx % 15), 45 + (by % 10), 38, 255))
            # Mold / moss
            if (row + col) % 3 == 0:
                for _ in range(12):
                    mx = bx + random.randint(3, 27)
                    my = by + random.randint(3, 12)
                    if 0 <= mx < 128 and 0 <= my < 128:
                        draw.point((mx, my), fill=(38, 55, 32, 255))
    img.save(path)
    print(f"Generated {path}")

def gen_attic_dust(path):
    img = Image.new("RGBA", (128, 128), (48, 44, 40, 255))
    draw = ImageDraw.Draw(img)
    for x in range(0, 128, 24):
        draw.rectangle([x, 0, x + 22, 127], fill=(58 + (x % 12), 52 + (x % 10), 44, 255))
        draw.line([x + 23, 0, x + 23, 127], fill=(20, 18, 16, 255))
    # Cobwebs
    for _ in range(140):
        draw.point((random.randint(0, 127), random.randint(0, 127)), fill=(180, 175, 170, 140))
    img.save(path)
    print(f"Generated {path}")

def gen_garage_wall(path):
    img = Image.new("RGBA", (128, 128), (60, 62, 65, 255))
    draw = ImageDraw.Draw(img)
    for x in range(128):
        for y in range(128):
            n = (x * 17 + y * 31) % 23
            draw.point((x, y), fill=(55 + n, 57 + n, 60 + n, 255))
    # Oil stain
    for dx in range(-25, 25):
        for dy in range(-20, 20):
            if dx*dx + dy*dy * 1.5 < 400:
                nx, ny = 64 + dx, 74 + dy
                if 0 <= nx < 128 and 0 <= ny < 128:
                    draw.point((nx, ny), fill=(22, 20, 18, 220))
    img.save(path)
    print(f"Generated {path}")

def gen_door_interior(path):
    img = Image.new("RGBA", (128, 128), (145, 138, 128, 255))
    draw = ImageDraw.Draw(img)
    draw.rectangle([0, 0, 127, 127], outline=(55, 48, 40, 255), width=3)
    # 4 panels
    for py in [12, 68]:
        for px in [12, 68]:
            draw.rectangle([px, py, px + 48, py + 48], outline=(85, 78, 68, 255), width=2)
            draw.rectangle([px + 3, py + 3, px + 45, py + 45], fill=(125, 118, 108, 255))
    # Brass handle
    draw.rectangle([112, 60, 122, 68], fill=(195, 150, 45, 255))
    draw.ellipse([110, 62, 114, 66], fill=(235, 195, 80, 255))
    img.save(path)
    print(f"Generated {path}")

def gen_workbench(path):
    img = Image.new("RGBA", (128, 128), (42, 32, 24, 255))
    draw = ImageDraw.Draw(img)
    draw.rectangle([6, 6, 121, 121], fill=(55, 42, 30, 255), outline=(120, 95, 60, 255), width=3)
    # Vise on left
    draw.rectangle([16, 20, 36, 50], fill=(70, 75, 85, 255))
    draw.rectangle([20, 48, 32, 75], fill=(110, 115, 125, 255))
    # Blueprint paper in center
    draw.rectangle([48, 24, 110, 96], fill=(28, 55, 95, 255), outline=(140, 180, 240, 255))
    # Shotgun schematic lines
    draw.line([56, 60, 102, 60], fill=(200, 225, 255, 255), width=2)
    draw.line([56, 60, 72, 75], fill=(200, 225, 255, 255), width=2)
    img.save(path)
    print(f"Generated {path}")

def gen_shotgun_parts(dir_path):
    # Barrel
    img_b = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    d_b = ImageDraw.Draw(img_b)
    d_b.rectangle([8, 26, 56, 32], fill=(35, 38, 45, 255), outline=(80, 85, 95, 255))
    d_b.rectangle([8, 33, 56, 39], fill=(28, 30, 36, 255), outline=(70, 75, 85, 255))
    d_b.rectangle([34, 39, 52, 46], fill=(95, 62, 35, 255)) # Wooden handguard
    img_b.save(os.path.join(dir_path, "item_shotgun_barrel.png"))

    # Stock
    img_s = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    d_s = ImageDraw.Draw(img_s)
    d_s.polygon([(12, 16), (28, 24), (54, 28), (54, 38), (34, 44), (12, 48)], fill=(110, 68, 38, 255), outline=(60, 35, 18, 255))
    d_s.rectangle([10, 16, 14, 48], fill=(30, 30, 35, 255)) # Rubber buttpad
    img_s.save(os.path.join(dir_path, "item_shotgun_stock.png"))

    # Trigger
    img_t = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    d_t = ImageDraw.Draw(img_t)
    d_t.rectangle([18, 20, 46, 38], fill=(55, 58, 65, 255), outline=(95, 100, 110, 255))
    d_t.arc([24, 36, 40, 52], start=0, end=180, fill=(80, 85, 95, 255), width=2)
    d_t.line([32, 38, 30, 46], fill=(180, 160, 80, 255), width=2)
    img_t.save(os.path.join(dir_path, "item_shotgun_trigger.png"))

    # Ammo box
    img_a = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    d_a = ImageDraw.Draw(img_a)
    d_a.rectangle([14, 18, 50, 48], fill=(160, 40, 35, 255), outline=(210, 175, 60, 255), width=2)
    d_a.text((18, 24), "12K", fill=(244, 220, 120, 255))
    img_a.save(os.path.join(dir_path, "item_shotgun_ammo.png"))

    # Crowbar
    img_c = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    d_c = ImageDraw.Draw(img_c)
    d_c.line([14, 52, 48, 16], fill=(140, 45, 40, 255), width=4) # Red hex bar
    d_c.arc([42, 10, 56, 24], start=180, end=340, fill=(75, 80, 90, 255), width=4) # Curved claw
    img_c.save(os.path.join(dir_path, "item_crowbar.png"))

    # Cutters
    img_w = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    d_w = ImageDraw.Draw(img_w)
    d_w.line([18, 52, 32, 32], fill=(40, 140, 60, 255), width=4)
    d_w.line([46, 52, 32, 32], fill=(40, 140, 60, 255), width=4)
    d_w.polygon([(32, 32), (24, 16), (32, 20), (40, 16)], fill=(120, 125, 135, 255))
    img_w.save(os.path.join(dir_path, "item_wire_cutters.png"))

    # Car battery
    img_cb = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    d_cb = ImageDraw.Draw(img_cb)
    d_cb.rectangle([14, 20, 50, 52], fill=(30, 32, 38, 255), outline=(70, 75, 85, 255))
    d_cb.rectangle([18, 14, 26, 20], fill=(160, 40, 40, 255)) # + terminal
    d_cb.rectangle([38, 14, 46, 20], fill=(40, 70, 140, 255)) # - terminal
    img_cb.save(os.path.join(dir_path, "item_car_battery.png"))

    # Sparkplug
    img_sp = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    d_sp = ImageDraw.Draw(img_sp)
    d_sp.rectangle([28, 12, 36, 32], fill=(230, 230, 235, 255)) # Ceramic white
    d_sp.rectangle([26, 32, 38, 46], fill=(110, 115, 125, 255)) # Threaded steel
    d_sp.line([32, 46, 32, 54], fill=(180, 185, 195, 255), width=2)
    img_sp.save(os.path.join(dir_path, "item_car_sparkplug.png"))

# ---------------------------------------------------------------------------
# 2. AUDIO SYNTHESIZERS (WAV 44100 Hz, 16-bit Mono)
# ---------------------------------------------------------------------------

def write_wav(filename, samples, sample_rate=44100):
    filepath = os.path.join(SFX_DIR, filename)
    with wave.open(filepath, 'w') as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        packed = struct.pack(f'<{len(samples)}h', *[max(-32767, min(32767, int(s))) for s in samples])
        wf.writeframes(packed)
    print(f"Synthesized {filepath} ({len(samples)/sample_rate:.2f}s)")

def gen_audio():
    sr = 44100

    # 1. Floor creak 1
    samples = []
    dur = 0.75
    total = int(sr * dur)
    for i in range(total):
        t = i / sr
        freq = 110.0 + math.sin(t * 32.0) * 45.0 + math.sin(t * 75.0) * 20.0
        env = math.exp(-t * 3.5) * (1.0 - math.exp(-t * 25.0))
        noise = (random.random() * 2.0 - 1.0) * 0.15
        s = (math.sin(2.0 * math.pi * freq * t) + noise) * env * 24000
        samples.append(s)
    write_wav("sfx_ch3_creak_wood_01.wav", samples, sr)

    # 2. Floor creak 2 (sharp)
    samples = []
    dur = 0.55
    total = int(sr * dur)
    for i in range(total):
        t = i / sr
        freq = 185.0 + math.sin(t * 55.0) * 60.0
        env = math.exp(-t * 5.0) * (1.0 - math.exp(-t * 40.0))
        noise = (random.random() * 2.0 - 1.0) * 0.22
        s = (math.sin(2.0 * math.pi * freq * t) + noise) * env * 26000
        samples.append(s)
    write_wav("sfx_ch3_creak_wood_02.wav", samples, sr)

    # 3. Crowbar pry
    samples = []
    dur = 1.1
    total = int(sr * dur)
    for i in range(total):
        t = i / sr
        wood_crack = (random.random() * 2.0 - 1.0) * math.exp(-t * 4.0) * 0.6
        metal_creak = math.sin(2.0 * math.pi * (240.0 + t * 380.0) * t) * math.exp(-t * 3.0) * 0.4
        thud = math.sin(2.0 * math.pi * 65.0 * t) * math.exp(-t * 7.0) * 0.5
        s = (wood_crack + metal_creak + thud) * 27000
        samples.append(s)
    write_wav("sfx_ch3_crowbar_pry.wav", samples, sr)

    # 4. Wire cutters
    samples = []
    dur = 0.4
    total = int(sr * dur)
    for i in range(total):
        t = i / sr
        snip = (random.random() * 2.0 - 1.0) * math.exp(-t * 35.0) * 0.8
        click = math.sin(2.0 * math.pi * 1800.0 * t) * math.exp(-t * 45.0) * 0.6
        spark = (random.random() * 2.0 - 1.0) * math.exp(-t * 12.0) * 0.3
        s = (snip + click + spark) * 28000
        samples.append(s)
    write_wav("sfx_ch3_wire_cut.wav", samples, sr)

    # 5. Shotgun assemble
    samples = []
    dur = 0.85
    total = int(sr * dur)
    for i in range(total):
        t = i / sr
        c1 = math.sin(2.0 * math.pi * 850.0 * t) * math.exp(-t * 25.0) if t < 0.25 else 0.0
        t2 = t - 0.28
        c2 = math.sin(2.0 * math.pi * 1200.0 * t2) * math.exp(-t2 * 30.0) if t2 > 0 else 0.0
        t3 = t - 0.55
        c3 = (math.sin(2.0 * math.pi * 420.0 * t3) + (random.random() - 0.5) * 0.5) * math.exp(-t3 * 20.0) if t3 > 0 else 0.0
        s = (c1 + c2 + c3) * 26000
        samples.append(s)
    write_wav("sfx_ch3_shotgun_assemble.wav", samples, sr)

    # 6. Shotgun blast
    samples = []
    dur = 1.6
    total = int(sr * dur)
    for i in range(total):
        t = i / sr
        boom = math.sin(2.0 * math.pi * (110.0 - t * 45.0) * t) * math.exp(-t * 4.5)
        blast = (random.random() * 2.0 - 1.0) * math.exp(-t * 6.0)
        reverb = (random.random() * 2.0 - 1.0) * math.exp(-t * 1.8) * 0.25
        s = (boom * 0.8 + blast * 0.9 + reverb) * 31000
        samples.append(s)
    write_wav("sfx_ch3_shotgun_blast.wav", samples, sr)

    # 7. Entity stun (shriek and heavy collapse)
    samples = []
    dur = 1.8
    total = int(sr * dur)
    for i in range(total):
        t = i / sr
        shriek_freq = 480.0 + math.sin(t * 60.0) * 180.0 - t * 200.0
        shriek = (math.sin(2.0 * math.pi * shriek_freq * t) + (random.random() - 0.5) * 0.4) * math.exp(-t * 3.0)
        t_hit = t - 0.45
        thud = math.sin(2.0 * math.pi * 55.0 * t_hit) * math.exp(-t_hit * 8.0) if t_hit > 0 else 0.0
        groan = math.sin(2.0 * math.pi * 95.0 * t) * math.exp(-t * 1.5) * 0.3
        s = (shriek * 0.7 + thud * 0.8 + groan) * 28000
        samples.append(s)
    write_wav("sfx_ch3_entity_stun.wav", samples, sr)

    # 8. Item drop (alarm sound for Granny AI)
    samples = []
    dur = 0.8
    total = int(sr * dur)
    for i in range(total):
        t = i / sr
        clatter = (random.random() * 2.0 - 1.0) * math.exp(-t * 15.0) * 0.8
        ring = math.sin(2.0 * math.pi * 2100.0 * t) * math.exp(-t * 8.0) * 0.5
        t_bounce = t - 0.14
        bounce = (random.random() * 2.0 - 1.0) * math.exp(-t_bounce * 20.0) * 0.4 if t_bounce > 0 else 0.0
        s = (clatter + ring + bounce) * 26000
        samples.append(s)
    write_wav("sfx_ch3_item_drop.wav", samples, sr)

    # 9. Car rev and garage smash
    samples = []
    dur = 2.4
    total = int(sr * dur)
    for i in range(total):
        t = i / sr
        crank = math.sin(2.0 * math.pi * 85.0 * t) * (1.0 if t < 0.6 else 0.0)
        rev = math.sin(2.0 * math.pi * (120.0 + t * 90.0) * t) * 0.6 if t >= 0.5 else 0.0
        t_crash = t - 1.2
        crash = (random.random() * 2.0 - 1.0) * math.exp(-t_crash * 3.5) if t_crash > 0 else 0.0
        s = (crank * 0.5 + rev + crash * 0.9) * 29000
        samples.append(s)
    write_wav("sfx_ch3_car_rev_smash.wav", samples, sr)

    # 10. Padlock open
    samples = []
    dur = 0.5
    total = int(sr * dur)
    for i in range(total):
        t = i / sr
        click = math.sin(2.0 * math.pi * 1400.0 * t) * math.exp(-t * 30.0)
        clang = math.sin(2.0 * math.pi * 320.0 * t) * math.exp(-t * 8.0) * 0.7
        s = (click + clang) * 25000
        samples.append(s)
    write_wav("sfx_ch3_padlock_open.wav", samples, sr)

    # 11. Keypad beep
    samples = []
    dur = 0.35
    total = int(sr * dur)
    for i in range(total):
        t = i / sr
        beep = math.sin(2.0 * math.pi * 1760.0 * t) * (1.0 if t < 0.15 else 0.0)
        relay = math.sin(2.0 * math.pi * 440.0 * (t - 0.18)) * math.exp(-(t - 0.18) * 25.0) if t > 0.18 else 0.0
        s = (beep * 0.6 + relay * 0.8) * 22000
        samples.append(s)
    write_wav("sfx_ch3_keypad_beep.wav", samples, sr)

if __name__ == "__main__":
    gen_wallpaper(os.path.join(TEX_DIR, "house_wallpaper_peeled.png"))
    gen_parquet(os.path.join(TEX_DIR, "house_floor_parquet.png"))
    gen_planks(os.path.join(TEX_DIR, "house_wood_planks.png"))
    gen_basement_brick(os.path.join(TEX_DIR, "house_basement_brick.png"))
    gen_attic_dust(os.path.join(TEX_DIR, "house_attic_dust.png"))
    gen_garage_wall(os.path.join(TEX_DIR, "house_garage_wall.png"))
    gen_door_interior(os.path.join(TEX_DIR, "house_door_interior.png"))
    gen_workbench(os.path.join(TEX_DIR, "house_workbench.png"))
    gen_shotgun_parts(TEX_DIR)
    gen_audio()
    print("ALL CHAPTER 3 ASSETS GENERATED SUCCESSFULLY!")

