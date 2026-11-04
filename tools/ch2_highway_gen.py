"""Chapter 2 map builder: turns the motel car into a drivable func_vehicle and appends the
1 km highway M-86 with the police post (ПОСТ ДПС) to assets/data/maps/ch2_motel86.map.

Run from the repo root:  py tools/ch2_highway_gen.py
The script is idempotent: every entity it writes carries "_lo_gen" "1" and is replaced on re-run.
World space here is the game's (x right, y up, z = -quake_y); 1 m = 32 map units.
"""
import math
import random
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MAP = ROOT / "assets" / "data" / "maps" / "ch2_motel86.map"
U = 32.0

ROAD_HALF = 4.0
SHOULDER = 7.0
GROUND = 30.0
WALL_IN = 12.0
WALL_OUT = 13.0
POST_S = 1000.0
POST_ZONE_S = 920.0
POST_WALL = 26.0


# ---------------------------------------------------------------------------- vector helpers
def sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def mul(a, k):
    return (a[0] * k, a[1] * k, a[2] * k)


def dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def norm(a):
    l = math.sqrt(dot(a, a))
    return (a[0] / l, a[1] / l, a[2] / l)


def to_quake(p):
    return (p[0] * U, -p[2] * U, p[1] * U)


def axis_to_quake(d):
    return (d[0], -d[2], d[1])


def num(v):
    s = f"{v:.3f}".rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s


# ---------------------------------------------------------------------------- brush writer
def face_line(point, normal, tex, uv):
    n = norm(normal)
    helper = (0.0, 1.0, 0.0) if abs(n[1]) < 0.9 else (1.0, 0.0, 0.0)
    b = norm(cross(n, helper))
    a = cross(b, n)
    p0 = point
    p1 = add(p0, mul(b, 2.0))
    p2 = add(p0, mul(a, 2.0))
    pts = " ".join("( " + " ".join(num(c) for c in to_quake(p)) + " )" for p in (p0, p1, p2))
    if uv is None:
        if abs(n[1]) > 0.7:
            uv = ((1, 0, 0), (0, 0, 1), 32.0, 32.0, 0.0, 0.0)
        elif abs(n[0]) > abs(n[2]):
            uv = ((0, 0, 1), (0, -1, 0), 32.0, 32.0, 0.0, 0.0)
        else:
            uv = ((1, 0, 0), (0, -1, 0), 32.0, 32.0, 0.0, 0.0)
    ud, vd, tu, tv, uo, vo = uv
    qa, qb = axis_to_quake(norm(ud)), axis_to_quake(norm(vd))
    return (f"{pts} {tex} [ {num(qa[0])} {num(qa[1])} {num(qa[2])} {num(uo)} ] "
            f"[ {num(qb[0])} {num(qb[1])} {num(qb[2])} {num(vo)} ] 0 {num(U / tu)} {num(U / tv)}")


def prism(foot, y0, y1, tex, top=None, bottom=None, face_uv=None):
    """foot: convex list of (x, z). Returns brush text."""
    cx = sum(p[0] for p in foot) / len(foot)
    cz = sum(p[1] for p in foot) / len(foot)
    lines = ["{"]
    lines.append(face_line((foot[0][0], y1, foot[0][1]), (0, 1, 0), top or tex, None))
    lines.append(face_line((foot[0][0], y0, foot[0][1]), (0, -1, 0), bottom or tex, None))
    for i in range(len(foot)):
        a, b = foot[i], foot[(i + 1) % len(foot)]
        dx, dz = b[0] - a[0], b[1] - a[1]
        n = (dz, 0.0, -dx)
        mx, mz = (a[0] + b[0]) * 0.5, (a[1] + b[1]) * 0.5
        if n[0] * (mx - cx) + n[2] * (mz - cz) < 0:
            n = (-n[0], 0.0, -n[2])
        uv = face_uv(n, a, b) if face_uv else None
        t = tex
        if isinstance(uv, tuple) and len(uv) == 2:
            t, uv = uv
        lines.append(face_line((a[0], y0, a[1]), n, t, uv))
    lines.append("}")
    return "\n".join(lines)


def box(x0, x1, y0, y1, z0, z1, tex, **kw):
    return prism([(x0, z0), (x1, z0), (x1, z1), (x0, z1)], y0, y1, tex, **kw)


def entity(kv, brushes=()):
    out = ["{"]
    for k, v in kv:
        out.append(f'"{k}" "{v}"')
    out.extend(brushes)
    out.append("}")
    return "\n".join(out)


# ---------------------------------------------------------------------------- road path
def build_path():
    nodes = [(30.0, 20.0, 0.0, 0.0)]  # x, z, heading(rad), s

    def straight(length, step):
        while length > 1e-3:
            d = min(step, length)
            x, z, h, s = nodes[-1]
            nodes.append((x + math.cos(h) * d, z + math.sin(h) * d, h, s + d))
            length -= d

    def arc(turn_deg, length, step=5.0):
        count = int(round(length / step))
        dh = math.radians(turn_deg) / count
        for _ in range(count):
            x, z, h, s = nodes[-1]
            hm = h + dh * 0.5
            nodes.append((x + math.cos(hm) * step, z + math.sin(hm) * step, h + dh, s + step))

    straight(220.0, 40.0)
    arc(18.0, 90.0)
    straight(200.0, 40.0)
    arc(-18.0, 90.0)
    x, z, h, s = nodes[-1]
    nodes[-1] = (x, z, 0.0, s)
    straight(1100.0 - nodes[-1][3], 40.0)
    return nodes


def point_at(nodes, s):
    for i in range(len(nodes) - 1):
        a, b = nodes[i], nodes[i + 1]
        if a[3] <= s <= b[3]:
            t = (s - a[3]) / max(1e-6, b[3] - a[3])
            h = math.atan2(b[1] - a[1], b[0] - a[0])
            return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, h)
    a, b = nodes[-2], nodes[-1]
    return (b[0], b[1], math.atan2(b[1] - a[1], b[0] - a[0]))


def offset(x, z, h, lateral, along=0.0):
    return (x + math.cos(h) * along - math.sin(h) * lateral, z + math.sin(h) * along + math.cos(h) * lateral)


def miter_normals(nodes):
    out = []
    for i in range(len(nodes)):
        hs = []
        if i > 0:
            hs.append(math.atan2(nodes[i][1] - nodes[i - 1][1], nodes[i][0] - nodes[i - 1][0]))
        if i + 1 < len(nodes):
            hs.append(math.atan2(nodes[i + 1][1] - nodes[i][1], nodes[i + 1][0] - nodes[i][0]))
        nx = sum(-math.sin(h) for h in hs) / len(hs)
        nz = sum(math.cos(h) for h in hs) / len(hs)
        l = math.hypot(nx, nz)
        nx, nz = nx / l, nz / l
        cosd = nx * -math.sin(hs[0]) + nz * math.cos(hs[0])
        out.append((nx / cosd, nz / cosd))
    return out


def strip(nodes, normals, i, l0, l1):
    a, b = nodes[i], nodes[i + 1]
    na, nb = normals[i], normals[i + 1]
    return [(a[0] + na[0] * l0, a[1] + na[1] * l0), (b[0] + nb[0] * l0, b[1] + nb[1] * l0),
            (b[0] + nb[0] * l1, b[1] + nb[1] * l1), (a[0] + na[0] * l1, a[1] + na[1] * l1)]


def oriented_box(cx, cz, h, half_along, half_lat, y0, y1, tex, **kw):
    pts = [offset(cx, cz, h, la, al) for al, la in ((-half_along, -half_lat), (half_along, -half_lat),
                                                     (half_along, half_lat), (-half_along, half_lat))]
    return prism(pts, y0, y1, tex, **kw)


# ---------------------------------------------------------------------------- content
def road_geometry(nodes):
    normals = miter_normals(nodes)
    solid, detail = [], []
    for i in range(len(nodes) - 1):
        s_mid = (nodes[i][3] + nodes[i + 1][3]) * 0.5
        solid.append(prism(strip(nodes, normals, i, -ROAD_HALF, ROAD_HALF), -0.5, 0.0, "motel_asphalt"))
        for side in (-1, 1):
            l0, l1 = sorted((side * ROAD_HALF, side * SHOULDER))
            solid.append(prism(strip(nodes, normals, i, l0, l1), -0.5, -0.03, "motel_gravel"))
            g0, g1 = sorted((side * SHOULDER, side * GROUND))
            solid.append(prism(strip(nodes, normals, i, g0, g1), -0.5, -0.06, "motel_grass"))
            e0, e1 = sorted((side * 3.62, side * 3.77))
            detail.append(prism(strip(nodes, normals, i, e0, e1), 0.0, 0.012, "motel_paint"))
            wall_in = POST_WALL if (side > 0 and s_mid > POST_ZONE_S) else WALL_IN
            w0, w1 = sorted((side * wall_in, side * (wall_in + 1.0)))
            solid.append(prism(strip(nodes, normals, i, w0, w1), -1.0, 7.0, "clip"))
    # post zone: connect the north wall jog and close the end of the road
    x, z, h, s = next(n for n in nodes if abs(n[3] - POST_ZONE_S) < 1e-3)
    solid.append(box(x - 0.5, x + 0.5, -1.0, 7.0, z + WALL_IN, z + POST_WALL + 1.0, "clip"))
    xe, ze, _, _ = nodes[-1]
    solid.append(box(xe, xe + 1.0, -1.0, 7.0, ze - WALL_OUT, ze + POST_WALL + 1.0, "clip"))
    # dashed centre line
    s = 6.0
    while s < nodes[-1][3] - 4.0:
        x, z, h = point_at(nodes, s + 1.5)
        detail.append(oriented_box(x, z, h, 1.5, 0.07, 0.0, 0.014, "motel_paint"))
        s += 9.0
    return solid, detail


def motel_link_walls():
    # Corridor between the motel lot (its east clip used to span x=13.5..14 over the whole lot)
    # and the generated road starting at x=30.
    return [
        box(13.5, 14.5, -1.0, 8.0, -30.0, 8.0, "clip"),
        box(13.5, 30.0, -1.0, 7.0, 7.0, 8.0, "clip"),
        box(13.5, 30.0, -1.0, 7.0, 32.0, 33.0, "clip"),
        box(13.0, 14.0, -1.0, 8.0, 26.0, 33.0, "clip"),
    ]


def sign(nodes, s, lateral, tex, solid, detail, width=2.4, height=1.2, bottom=1.6):
    x, z, h = point_at(nodes, s)
    cx, cz = offset(x, z, h, lateral)
    fwd = (math.cos(h), 0.0, math.sin(h))
    right = (-math.sin(h), 0.0, math.cos(h))
    top = bottom + height
    left_edge = (cx - right[0] * width * 0.5, top, cz - right[2] * width * 0.5)
    tu, tv = 128.0 / width, 64.0 / height

    def face_uv(n, a, b):
        if n[0] * -fwd[0] + n[2] * -fwd[2] > 0.9:
            return (tex, (right, (0, -1, 0), tu, tv, -dot(left_edge, right) * tu, top * tv))
        return ("motel_metal", None)

    detail.append(oriented_box(cx, cz, h, 0.04, width * 0.5, bottom, top, "motel_metal", face_uv=face_uv))
    for side in (-1, 1):
        px, pz = offset(cx, cz, h, side * (width * 0.5 - 0.25), 0.08)
        solid.append(oriented_box(px, pz, h, 0.05, 0.05, -0.1, top - 0.1, "motel_metal"))


def poles(nodes, rng, solid, detail):
    tops = []
    s = 18.0
    while s < POST_S - 30.0:
        x, z, h = point_at(nodes, s)
        px, pz = offset(x, z, h, -9.0)
        solid.append(oriented_box(px, pz, h, 0.13, 0.13, -0.2, 7.2, "motel_wall_wood", top="motel_wall_wood"))
        detail.append(oriented_box(px, pz, h, 0.08, 0.9, 6.7, 6.85, "motel_wall_wood"))
        tops.append((px, pz, h))
        s += 46.0 + rng.uniform(-3.0, 3.0)
    for a, b in zip(tops, tops[1:]):
        for lat in (-0.75, 0.0, 0.75):
            ax, az = offset(a[0], a[1], a[2], lat)
            bx, bz = offset(b[0], b[1], b[2], lat)
            h = math.atan2(bz - az, bx - ax)
            length = math.hypot(bx - ax, bz - az)
            detail.append(oriented_box((ax + bx) * 0.5, (az + bz) * 0.5, h, length * 0.5, 0.012, 6.86, 6.885, "motel_metal"))


def trees(nodes, rng):
    out = []
    for side in (-1, 1):
        s = 4.0
        while s < nodes[-1][3] - 2.0:
            for lo, hi in ((9.5, 14.0), (15.5, 29.0)):
                if side > 0 and s > POST_ZONE_S - 20.0 and lo < POST_WALL + 1.0:
                    continue
                lat = side * rng.uniform(lo, hi)
                x, z, h = point_at(nodes, s + rng.uniform(-2.5, 2.5))
                tx, tz = offset(x, z, h, lat)
                out.append(entity([("classname", "prop_tree"), ("origin", " ".join(num(c) for c in to_quake((tx, 0.0, tz)))),
                                   ("angle", str(rng.randint(0, 359))), ("scale", num(rng.uniform(0.8, 1.35))), ("_lo_gen", "1")]))
            s += rng.uniform(5.5, 9.0)
    return out


def police_post(nodes, solid, detail):
    x0, zc, _ = point_at(nodes, POST_S)
    zb = zc + 10.0
    xp = x0
    ents = []
    # parking pad and booth (door + windows face the road, i.e. -z)
    solid.append(box(xp - 26.0, xp + 22.0, -0.5, 0.0, zc + 4.0, zc + 21.0, "motel_asphalt"))
    solid.append(box(xp - 3.2, xp + 3.2, 0.0, 0.05, zb, zb + 4.6, "motel_concrete", top="motel_tile"))
    t = 0.2
    yt = 2.8
    wall = "motel_concrete"
    solid.append(box(xp - 3.2, xp + 3.2, 0.0, yt, zb + 4.4, zb + 4.6, wall))      # back
    solid.append(box(xp - 3.2, xp - 3.0, 0.0, yt, zb, zb + 4.6, wall))            # west
    solid.append(box(xp + 3.0, xp + 3.2, 0.0, yt, zb, zb + 4.6, wall))            # east
    solid.append(box(xp - 3.0, xp + 1.0, 0.0, 1.0, zb, zb + t, wall))             # front, under window
    solid.append(box(xp - 3.0, xp - 2.6, 1.0, 2.1, zb, zb + t, wall))
    solid.append(box(xp - 2.6, xp + 1.0, 1.0, 2.1, zb + 0.08, zb + 0.12, "motel_window"))
    solid.append(box(xp + 1.0, xp + 1.2, 0.0, 2.1, zb, zb + t, wall))
    solid.append(box(xp + 2.2, xp + 3.0, 0.0, 2.1, zb, zb + t, wall))             # door gap x in [1.2, 2.2]
    solid.append(box(xp - 3.0, xp + 3.0, 2.1, yt, zb, zb + t, wall))
    solid.append(box(xp - 3.6, xp + 3.6, yt, yt + 0.2, zb - 0.6, zb + 5.0, "motel_roof"))
    sign_top = yt + 0.85

    def dps_uv(n, a, b):
        if n[2] < -0.9:
            return ("motel_dps_sign", ((-1, 0, 0), (0, -1, 0), 40.0, 32.0 / 0.65, (xp + 1.6) * 40.0, sign_top * 32.0 / 0.65))
        return ("motel_metal", None)

    detail.append(box(xp - 1.6, xp + 1.6, yt + 0.2, sign_top, zb - 0.1, zb, "motel_metal", face_uv=dps_uv))
    solid.append(box(xp - 2.5, xp - 0.3, 0.0, 0.78, zb + 3.0, zb + 4.3, "motel_wall_wood"))  # table
    solid.append(box(xp + 1.8, xp + 2.9, 0.0, 1.9, zb + 3.6, zb + 4.4, "motel_metal"))      # locker
    # barrier across the road and concrete blocks behind it
    bx = xp + 9.0
    solid.append(box(bx - 0.2, bx + 0.2, 0.0, 1.2, zc - 5.0, zc - 4.6, "motel_metal"))
    solid.append(box(bx - 0.08, bx + 0.08, 0.9, 1.06, zc - 4.6, zc + 4.8, "motel_barrier"))
    solid.append(box(bx - 0.2, bx + 0.2, 0.0, 1.1, zc + 4.8, zc + 5.2, "motel_metal"))
    for k in range(-3, 4):
        solid.append(box(xp + 40.0, xp + 41.2, 0.0, 0.9, zc + k * 1.25 - 0.55, zc + k * 1.25 + 0.55, "motel_concrete"))
    # parked patrol car (static, dead battery)
    solid.append(box(xp - 15.0, xp - 10.5, 0.3, 1.0, zc + 15.0, zc + 17.0, "motel_car"))
    solid.append(box(xp - 14.2, xp - 11.6, 1.0, 1.5, zc + 15.15, zc + 16.85, "motel_car", top="motel_car"))
    detail.append(box(xp - 13.4, xp - 12.6, 1.5, 1.62, zc + 15.6, zc + 16.4, "motel_metal"))
    # floodlight mast
    solid.append(box(xp - 6.15, xp - 5.85, -0.2, 5.2, zc + 7.35, zc + 7.65, "motel_metal"))
    detail.append(box(xp - 6.5, xp - 5.5, 5.0, 5.35, zc + 7.0, zc + 7.6, "motel_metal"))

    def light(pos, color, radius, intensity, style, dynamic):
        kv = [("classname", "light"), ("origin", " ".join(num(c) for c in to_quake(pos))), ("_color", color),
              ("radius", num(radius)), ("intensity", num(intensity)), ("style", style)]
        if dynamic:
            kv.append(("dynamic", "1"))
        kv.append(("_lo_gen", "1"))
        return entity(kv)

    ents.append(light((xp - 1.0, 2.55, zb + 2.2), "1 0.82 0.55", 5.5, 1.15, "steady", False))
    ents.append(light((xp - 1.0, 2.5, zb + 2.6), "1 0.8 0.5", 4.0, 0.45, "flicker", True))
    ents.append(light((xp - 6.0, 4.9, zc + 6.6), "0.82 0.9 1", 14.0, 1.1, "flicker", True))
    ents.append(light((bx, 1.3, zc - 4.8), "1 0.1 0.08", 3.5, 1.2, "blink", True))
    ents.append(light((xp + 0.5, 3.3, zb - 0.4), "0.75 0.85 1", 4.5, 0.7, "steady", False))

    def item(name, pos, angle):
        return entity([("classname", "item_pickup"), ("origin", " ".join(num(c) for c in to_quake(pos))),
                       ("item", name), ("angle", str(angle)), ("_lo_gen", "1")])

    ents.append(item("pistol", (xp - 1.7, 0.81, zb + 3.6), 200))
    ents.append(item("ammo", (xp - 0.9, 0.81, zb + 3.8), 120))
    ents.append(entity([("classname", "trigger_event"), ("event", "post_arrive"), ("once", "0"), ("_lo_gen", "1")],
                       [box(xp - 70.0, xp + 12.0, -1.0, 5.0, zc - 12.0, zc + 25.0, "trigger")]))
    return ents, (xp, zc, zb)


def marker(name, pos, yaw_deg_world):
    # QuakeYawToWorld: yaw = (90 - angle) deg  ->  angle = 90 - yaw
    return entity([("classname", "info_marker"), ("targetname", name),
                   ("origin", " ".join(num(c) for c in to_quake(pos))), ("angle", num(90.0 - yaw_deg_world)), ("_lo_gen", "1")])


# ---------------------------------------------------------------------------- map surgery
def split_entities(text):
    ents, depth, start = [], 0, None
    i = 0
    lines = text.splitlines(keepends=True)
    for idx, line in enumerate(lines):
        s = line.strip()
        if s == "{":
            if depth == 0:
                start = idx
            depth += 1
        elif s == "}":
            depth -= 1
            if depth == 0:
                ents.append((start, idx))
    return lines, ents


def brush_blocks(lines, a, b):
    out, depth, start = [], 0, None
    for idx in range(a, b + 1):
        s = lines[idx].strip()
        if s == "{":
            depth += 1
            if depth == 2:
                start = idx
        elif s == "}":
            if depth == 2:
                out.append((start, idx))
            depth -= 1
    return out


PLANE_RE = re.compile(r"\(\s*([-\d.e]+)\s+([-\d.e]+)\s+([-\d.e]+)\s*\)")


def brush_vertices(block_lines):
    planes = []
    for l in block_lines:
        pts = PLANE_RE.findall(l)
        if len(pts) < 3:
            continue
        p0, p1, p2 = [tuple(map(float, p)) for p in pts[:3]]
        n = norm(cross(sub(p2, p0), sub(p1, p0)))
        planes.append((n, dot(n, p0)))
    verts = []
    for i in range(len(planes)):
        for j in range(i + 1, len(planes)):
            for k in range(j + 1, len(planes)):
                a, b, c = planes[i], planes[j], planes[k]
                det = dot(a[0], cross(b[0], c[0]))
                if abs(det) < 1e-6:
                    continue
                p = mul(add(add(mul(cross(b[0], c[0]), a[1]), mul(cross(c[0], a[0]), b[1])), mul(cross(a[0], b[0]), c[1])), 1.0 / det)
                if all(dot(n, p) - d <= 0.05 for n, d in planes):
                    verts.append((p[0] / U, p[2] / U, -p[1] / U))
    return verts


def kv_of(lines, a, b):
    kv = {}
    for idx in range(a, b + 1):
        m = re.match(r'\s*"([^"]*)"\s+"([^"]*)"', lines[idx])
        if m:
            kv.setdefault(m.group(1), m.group(2))
    return kv


def main():
    text = MAP.read_text(encoding="utf-8")
    lines, ents = split_entities(text)
    header_end = ents[0][0]
    keep = []
    vehicle_brushes = []
    has_vehicle = False
    for a, b in ents:
        kv = kv_of(lines, a, b)
        if kv.get("_lo_gen") == "1":
            continue
        cls = kv.get("classname")
        if cls == "func_vehicle":
            has_vehicle = True
        if cls == "func_interact" and kv.get("event") == "car":
            continue
        if cls == "light":
            o = [float(v) for v in kv.get("origin", "0 0 0").split()]
            if any(math.dist(o[:2], c) < 6.0 for c in ((-201.6, -342.4), (-201.6, -297.6), (-32.0, -320.0))) and o[2] < 40.0:
                continue
            if o == [160.0, 160.0, 68.0] and "group" not in kv:
                block = "".join(lines[a:b + 1]).replace('"style" "steady"', '"style" "steady"\n"group" "power"', 1)
                keep.append(block)
                continue
        if cls == "worldspawn":
            out = []
            for ba, bb in brush_blocks(lines, a, b):
                v = brush_vertices(lines[ba:bb + 1])
                xs, ys, zs = [p[0] for p in v], [p[1] for p in v], [p[2] for p in v]
                in_car = v and min(xs) >= -6.2 and max(xs) <= -1.3 and min(zs) >= 8.8 and max(zs) <= 11.3 and max(ys) <= 1.6
                old_clip = v and abs(min(xs) - 13.5) < 0.01 and abs(max(xs) - 14.0) < 0.01 and min(zs) < -29.0 and max(zs) > 25.0
                road_block = v and min(xs) >= 12.4 and max(xs) <= 15.6 and min(zs) >= 15.4 and max(zs) <= 24.55 and max(ys) <= 0.5
                if in_car and not has_vehicle:
                    vehicle_brushes.append("".join(lines[ba:bb + 1]).rstrip("\n"))
                    out.append((ba, bb))
                elif old_clip or road_block:
                    out.append((ba, bb))
            skip = set()
            for ba, bb in out:
                skip.update(range(ba, bb + 1))
            keep.append("".join(lines[idx] for idx in range(a, b + 1) if idx not in skip))
            continue
        keep.append("".join(lines[a:b + 1]))

    rng = random.Random(86)
    nodes = build_path()
    solid, detail = road_geometry(nodes)
    solid.extend(motel_link_walls())
    sign(nodes, 26.0, 5.6, "motel_board_post1km", solid, detail)
    sign(nodes, 500.0, 5.6, "motel_board_post500", solid, detail)
    sign(nodes, POST_S - 70.0, 5.6, "motel_board_stop", solid, detail)
    poles(nodes, rng, solid, detail)
    post_ents, (xp, zc, zb) = police_post(nodes, solid, detail)
    # wrecked car on the shoulder halfway through the first straight
    x, z, h = point_at(nodes, 300.0)
    cx, cz = offset(x, z, h, -6.0)
    solid.append(oriented_box(cx, cz, h + 0.35, 2.25, 1.0, 0.25, 0.95, "motel_car"))
    solid.append(oriented_box(cx, cz, h + 0.35, 1.2, 0.85, 0.95, 1.4, "motel_car"))

    gen = []
    if vehicle_brushes:
        gen.append(entity([("classname", "func_vehicle"), ("targetname", "volga"), ("event", "car"),
                           ("label", "«Волга» ГАЗ-24"), ("angle", "0")], vehicle_brushes))
    gen.append(entity([("classname", "func_group"), ("_tb_name", "highway_m86"), ("_lo_gen", "1")], solid))
    gen.append(entity([("classname", "func_illusionary"), ("_tb_name", "highway_detail"), ("_lo_gen", "1")], detail))
    for n in nodes:
        gen.append(entity([("classname", "path_road"), ("origin", " ".join(num(c) for c in to_quake((n[0], 0.0, n[1])))),
                           ("s", num(n[3])), ("_lo_gen", "1")]))
    gen.extend(trees(nodes, rng))
    gen.extend(post_ents)
    gen.append(entity([("classname", "trigger_event"), ("event", "highway_enter"), ("once", "0"), ("_lo_gen", "1")],
                      [box(30.0, 40.0, -1.0, 5.0, 8.0, 32.0, "trigger")]))
    # entrance key appears on the key board (next to 101/102) once the power is restored after the blackout
    gen.append(entity([("classname", "item_pickup"), ("origin", "160 247.68 59.2"), ("item", "key_entrance"), ("angle", "270"),
                       ("require_flag", "power_restored"), ("_lo_gen", "1")]))
    gen.append(marker("stalker_spawn_yard", (-17.0, 0.05, -23.5), 45.0))
    gen.append(marker("checkpoint_generator", (-1.6, 0.05, -18.8), 180.0))
    gen.append(marker("stalker_spawn_porch", (1.5, 0.3, 2.5), 0.0))
    gen.append(marker("checkpoint_post", (xp - 1.0, 0.1, zb + 2.0), 0.0))
    gen.append(marker("stalker_spawn_post", (xp - 42.0, 0.05, zc - 1.0), 90.0))
    for i, s in enumerate((380.0, 730.0), start=1):
        ax, az, _ = point_at(nodes, s)
        gen.append(marker(f"ambush_{i}", (ax, 0.05, az), 0.0))

    header = "".join(lines[:header_end])
    MAP.write_text(header + "\n".join(k.rstrip("\n") for k in keep) + "\n" + "\n".join(gen) + "\n", encoding="utf-8")
    print(f"road nodes: {len(nodes)}  solid brushes: {len(solid)}  detail brushes: {len(detail)}")
    print(f"post at x={xp:.1f} z={zc:.1f}  road end x={nodes[-1][0]:.1f}")


if __name__ == "__main__":
    main()
