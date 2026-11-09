#!/usr/bin/env python3
"""
Druckteile für die Radio-Blende (10 mm Holz), offene Bauweise: jedes Teil wird einzeln
von hinten auf die Blende geschraubt.

Teile (jeweils in Drucklage, Z = 0 auf dem Druckbett):
  display_haube.stl   Haube hinter dem Display (Waveshare ESP32-S3-Touch-LCD-4),
                      zieht das Display mit 4 x M2.5 an die Blende.
  pi_gehaeuse.stl     Raspberry Pi 4 + Geekworm G341 + DACBerry AMP2, DAB-Stick, SMA.
  pi_deckel.stl       Deckel zum Pi-Gehäuse.
  drehgeber_platte.stl  Halteplatte für den Alps EC11 (M7-Gewindehülse).
  wago_halter.stl     2 x WAGO 221-415 (+12 V, Masse) mit Kabelbinder-Tunneln.
  shelly_halter.stl   Wiege für den Shelly 1 Gen3.

Alle Maße in mm. Benötigt manifold3d.
"""
import math
import os
import struct
import sys
import zipfile

import manifold3d as mf

SEG = 64
CLEAR = 0.3

# ============================================================================ Helfer
def box(x, y, z, dx, dy, dz):
    return mf.Manifold.cube((dx, dy, dz)).translate((x, y, z))


def cyl(x, y, z, d, h, d2=None):
    return mf.Manifold.cylinder(h, d / 2, (d2 if d2 is not None else d) / 2, SEG).translate((x, y, z))


def cyl_x(x, y, z, d, length):
    return mf.Manifold.cylinder(length, d / 2, d / 2, SEG).rotate((0, 90, 0)).translate((x, y, z))


def cyl_y(x, y, z, d, length):
    return mf.Manifold.cylinder(length, d / 2, d / 2, SEG).rotate((-90, 0, 0)).translate((x, y, z))


def rrect(x, y, dx, dy, r, h, z=0.0):
    cs = mf.CrossSection.square((dx - 2 * r, dy - 2 * r)).translate((r, r))
    cs = cs.offset(r, mf.JoinType.Round, 2.0, SEG)
    return mf.Manifold.extrude(cs, h).translate((x, y, z))


def union(parts):
    return mf.Manifold.batch_boolean(list(parts), mf.OpType.Add)


def slot_z(xc, yc, z, w, length, h, along_x=True):
    """Langloch senkrecht (durch eine waagrechte Platte)."""
    a = (length - w) / 2
    if along_x:
        return union([cyl(xc - a, yc, z, w, h), cyl(xc + a, yc, z, w, h), box(xc - a, yc - w / 2, z, 2 * a, w, h)])
    return union([cyl(xc, yc - a, z, w, h), cyl(xc, yc + a, z, w, h), box(xc - w / 2, yc - a, z, w, 2 * a, h)])


def screw_tab(xc, yc, h, w=14.0, l=16.0, along_x=True):
    """Schraublasche mit Langloch für Holzschraube 3,5 mm (Kopf bis 7 mm)."""
    tab = rrect(xc - (l if along_x else w) / 2, yc - (w if along_x else l) / 2, l if along_x else w,
                w if along_x else l, 3.0, h)
    return tab, slot_z(xc, yc, -1, 4.0, 9.0, h + 2, along_x)


def tie_tunnel(x, y, z, length, along_x=True, w=5.0, h=2.2):
    return box(x, y - w / 2, z, length, w, h) if along_x else box(x - w / 2, y, z, w, length, h)


# ============================================================================ 1) Display-Haube
# Display: Lochbild 66,5 x 68,5 mm, Gewindebuchsen M2.5 (4 mm hoch). Blendenausschnitt 78 x 80.
# Aus dem CamperUI-Dev-Case: Klebeband-Auflage -> Buchsen-Stirnfläche = 11,2 mm.
# Holz 10 mm, 0,2 mm Vorspannung -> Dome enden 1,0 mm hinter der Blendenrückseite.
PANEL_T = 10.0
DEV_TAPE_TO_BUSHING = 11.2
PRELOAD = 0.2
DOME_GAP = DEV_TAPE_TO_BUSHING - PANEL_T - PRELOAD          # 1,0 mm
HOLE_DX, HOLE_DY = 66.5, 68.5
H_IN_X, H_IN_Y = 78.0, 80.0         # = Blendenausschnitt (Platine 74,5 x 76,5 passt durch)
H_WALL = 2.4
H_DEPTH = 12.0                      # Innenraum hinter der Blende (Bauteile ca. 6 mm + Kabel)
H_FLOOR = 2.5
H_TOTAL = H_DEPTH + H_FLOOR         # 14,5
DOME_D, DOME_HOLE, DOME_CB, DOME_SEAT = 7.0, 2.9, 5.8, 3.0   # Sitz 3 mm unter dem Domkopf
TAB_D, WOOD_HOLE, WOOD_CB, WOOD_SEAT = 12.0, 4.0, 8.0, 3.0   # Eckpfosten für Holzschraube 3,5 x 10


def display_haube():
    ox, oy = H_IN_X + 2 * H_WALL, H_IN_Y + 2 * H_WALL       # 82,8 x 84,8
    cx, cy = ox / 2, oy / 2
    # Drucklage: Boden (Rückseite) auf dem Bett, Z = H_TOTAL liegt an der Blende.
    shell = rrect(0, 0, ox, oy, 3.0, H_TOTAL) - box(H_WALL, H_WALL, H_FLOOR, H_IN_X, H_IN_Y, H_TOTAL)
    corners = [(cx + sx * (ox / 2 + 3.0), cy + sy * (oy / 2 + 3.0)) for sx in (-1, 1) for sy in (-1, 1)]
    add = [shell]
    for px, py in corners:                                     # Eckpfosten, mit der Wand verbunden
        add.append(cyl(px, py, 0, TAB_D, H_TOTAL))
        add.append(box(min(px, cx + (px - cx) * 0.80) - 3, min(py, cy + (py - cy) * 0.80) - 3,
                       0, abs(px - cx) * 0.20 + 6, abs(py - cy) * 0.20 + 6, H_TOTAL))
    dome_top = H_TOTAL - DOME_GAP
    domes = [(cx + sx * HOLE_DX / 2, cy + sy * HOLE_DY / 2) for sx in (-1, 1) for sy in (-1, 1)]
    for dx, dy in domes:
        add.append(cyl(dx, dy, 0, DOME_D, dome_top))
    m = union(add)
    cut = []
    for px, py in corners:
        cut.append(cyl(px, py, -1, WOOD_HOLE, H_TOTAL + 2))
        cut.append(cyl(px, py, -1, WOOD_CB, H_TOTAL - WOOD_SEAT + 1))
    for dx, dy in domes:
        cut.append(cyl(dx, dy, -1, DOME_HOLE, H_TOTAL + 2))
        cut.append(cyl(dx, dy, -1, DOME_CB, dome_top - DOME_SEAT + 1))
    # Durchbrüche im Boden wie im Dev-Case (relativ zum Lochbild-Mittelpunkt)
    cut.append(box(cx + 31.1 - 6.4, cy + 4.2 - 10.5, -1, 12.8, 21.0, H_FLOOR + 2))
    cut.append(box(cx + 0.7 - 20.0, cy + 33.8 - 7.3, -1, 40.0, 14.6, H_FLOOR + 2))
    cut.append(rrect(cx - 18, cy - 24, 36, 30, 4, H_FLOOR + 2, -1))          # Belüftung Mitte
    # Kabelausgang an der Klemmen-Seite ("unten", +Y): Kerbe an der Blendenseite
    cut.append(box(cx - 20, oy - H_WALL - 1, H_TOTAL - 6.0, 40, H_WALL + 2, 7))
    # Markierung "unten": Kerbe außen am Boden
    cut.append(mf.Manifold.extrude(mf.CrossSection([[(cx + 4, oy + 0.01), (cx - 4, oy + 0.01), (cx, oy - 5)]]), 1.2)
               .translate((0, 0, -0.01)))
    return m - union(cut)


# ============================================================================ 2) Pi-Gehäuse
# Raspberry Pi 4 (85 x 56), Anschlüsse rechts (+X), HDMI/USB-C vorn (Y = 0),
# 40-Pin-Leiste hinten (+Y). Darauf G341 + DACBerry AMP2 (Stapelhöhe geschätzt).
STACK_H = 45.0          # Innenhöhe: Pi + G341 + DACBerry inkl. Klemmen ca. 39-41 mm + Luft
P_WALL, P_FLOOR, P_LID = 2.2, 2.5, 2.5
P_IN_X, P_IN_Y = 96.0, 72.0               # Stirnwand direkt hinter den Pi-Buchsen
PI_X, PI_Y = 8.0, 8.0                     # Pi-Ecke im Innenraum (Deckel-Dome in den Ecken frei)
PI_HOLES = [(3.5, 3.5), (61.5, 3.5), (3.5, 52.5), (61.5, 52.5)]
STANDOFF_H, STANDOFF_D, M25_TAP = 4.0, 6.0, 2.2
# DAB-Stick im oberen schwarzen USB-2-Port (Pi 4: USB 2.0 bei Y = 47 mm, oberer Port ca. 12 mm über der Platine).
# Der Stick ragt durch ein Fenster in der Stirnwand nach außen und liegt dort auf einer Zunge
# mit Auflage und Kabelbinder (Antenne direkt am Stick).
USB2_Y = 47.0
TONGUE_LEN, TONGUE_W = 62.0, 30.0
STICK_REST_DIST, STICK_REST_TOP = 40.0, 12.0     # Auflage 40 mm vor der Stirnwand, Oberkante 12 mm (Schaumpad 1-3 mm)
BOSS_D, BOSS_HOLE, BOSS_DEPTH = 7.0, 2.6, 12.0


def _p_dims():
    lx, ly = P_IN_X + 2 * P_WALL, P_IN_Y + 2 * P_WALL
    return lx, ly, P_FLOOR + STACK_H


def _p_bosses():
    lx, ly, _ = _p_dims()
    w = P_WALL
    return [(w + 3.5, w + 3.5), (lx - w - 3.5, w + 3.5), (w + 3.5, ly - w - 3.5), (lx - w - 3.5, ly - w - 3.5)]


def pi_gehaeuse():
    lx, ly, lz = _p_dims()
    x0, y0 = P_WALL, P_WALL
    body = rrect(0, 0, lx, ly, 4.0, lz) - box(x0, y0, P_FLOOR, P_IN_X, P_IN_Y, lz)
    add = [body]
    cut = []
    for bx, by in _p_bosses():
        add.append(cyl(bx, by, 0, BOSS_D, lz))
        cut.append(cyl(bx, by, lz - BOSS_DEPTH, BOSS_HOLE, BOSS_DEPTH + 1))
    for hx, hy in PI_HOLES:
        add.append(cyl(x0 + PI_X + hx, y0 + PI_Y + hy, P_FLOOR - 0.01, STANDOFF_D, STANDOFF_H + 0.01))
        cut.append(cyl(x0 + PI_X + hx, y0 + PI_Y + hy, P_FLOOR + 0.6, M25_TAP, STANDOFF_H + 1))
    # Zunge außen an der Stirnwand mit Stick-Auflage, Kabelbinder-Tunnel und Schraub-Langloch am Ende
    sy = y0 + PI_Y + USB2_Y
    add.append(rrect(lx - 4, sy - TONGUE_W / 2, TONGUE_LEN + 4, TONGUE_W, 3.0, 3.0))
    rx = lx + STICK_REST_DIST
    add.append(box(rx - 6, sy - 12, 0, 12, 24, STICK_REST_TOP))
    cut.append(box(rx - 2.5, sy - 13, STICK_REST_TOP - 5.5, 5, 26, 2.2))
    cut.append(slot_z(lx + TONGUE_LEN - 7, sy, -1, 4.0, 9.0, 5, along_x=True))
    # Schraublaschen am Boden (Blendenseite), Langlöcher in X
    for tx in (30.0, lx - 30.0):
        for ty, side in ((-7.0, -1), (ly + 7.0, 1)):
            tab, hole = screw_tab(tx, ty, 3.0)
            add.append(tab)
            add.append(box(tx - 7, -8.0 if side < 0 else ly - 1, 0, 14, 9.0, 3.0))
            cut.append(hole)
    m = union(add)
    # Belüftung unten vorn (Y = 0): senkrechte Schlitze
    for i in range(10):
        cut.append(box(x0 + 14 + i * 7, -1, P_FLOOR + 3, 3.0, P_WALL + 2, 12))
    # Kabelöffnung hinten (+Y, Seite der 40-Pin-Leiste): Lautsprecher, 12 V, Drehgeber
    cut.append(box(x0 + 12, ly - P_WALL - 1, P_FLOOR + 20.0, 72, P_WALL + 2, lz))
    # zweite Öffnung vorn oben (Klemmen der DACBerry, falls sie nach vorn zeigen)
    cut.append(box(x0 + 12, -1, P_FLOOR + 26.0, 60, P_WALL + 2, lz))
    # SD-Karte links
    cut.append(box(-1, y0 + PI_Y + 28 - 8, P_FLOOR + 0.5, P_WALL + 2, 16, 7))
    # Fenster in der Stirnwand für die USB-Buchsen (Stick steckt oben im schwarzen USB-2-Port)
    pcb_top = P_FLOOR + STANDOFF_H + 1.4
    cut.append(box(lx - P_WALL - 1, y0 + PI_Y + USB2_Y - 14, pcb_top - 0.5, P_WALL + 2, 28, 24))
    return m - union(cut)


def pi_deckel():
    lx, ly, _ = _p_dims()
    plate = rrect(0, 0, lx, ly, 4.0, P_LID)
    lip_o = box(P_WALL + CLEAR, P_WALL + CLEAR, P_LID, P_IN_X - 2 * CLEAR, P_IN_Y - 2 * CLEAR, 3.0)
    lip_i = box(P_WALL + CLEAR + 1.6, P_WALL + CLEAR + 1.6, P_LID - 0.1, P_IN_X - 2 * CLEAR - 3.2,
                P_IN_Y - 2 * CLEAR - 3.2, 3.4)
    lip = lip_o - lip_i
    # Deckel liegt mit der Außenseite auf dem Bett -> Y gespiegelt
    for bx, by in _p_bosses():
        lip = lip - cyl(bx, ly - by, P_LID - 0.5, BOSS_D + 2.4, 5)
    # Aussparungen im Rand für die Kabelöffnungen
    lip = lip - box(P_WALL + 12, -1, P_LID - 0.1, 72, 2 * P_WALL + 4, 4)
    lip = lip - box(P_WALL + 12, ly - 2 * P_WALL - 3, P_LID - 0.1, 60, 2 * P_WALL + 4, 4)
    lid = plate + lip
    cut = []
    for bx, by in _p_bosses():
        y = ly - by
        cut.append(cyl(bx, y, -1, 3.4, P_LID + 2))
        cut.append(mf.Manifold.cylinder(1.8, 3.4, 1.7, SEG).translate((bx, y, -0.01)))
    # Lüftungsschlitze über Pi/DACBerry und über dem Stick
    for i in range(12):
        cut.append(box(P_WALL + 14 + i * 7, ly / 2 - 20, -1, 3, 40, P_LID + 2))
    return lid - union(cut)


# ============================================================================ 3) Drehgeber-Platte
# EC11 mit M7x0,75-Hülse: Platte 2 mm liegt hinten an der Blende, Hülse ragt in eine
# Tasche (Ø 12, 6 mm tief, von hinten gefräst), Mutter sitzt in der Tasche.
ENC_PLATE_T = 2.0


def drehgeber_platte():
    w, l = 22.0, 52.0
    plate = rrect(-l / 2, -w / 2, l, w, 4.0, ENC_PLATE_T)
    cut = [cyl(0, 0, -1, 7.2, ENC_PLATE_T + 2)]
    for sx in (-1, 1):
        cut.append(slot_z(sx * 19.0, 0, -1, 4.0, 8.0, ENC_PLATE_T + 2, along_x=False))
    # Rahmen um den Drehgeberkörper (12 x 12) gegen Verdrehen, 4 mm hoch auf der Rückseite
    frame = box(-7.6, -7.6, ENC_PLATE_T - 0.01, 15.2, 15.2, 4.0) - box(-6.3, -6.3, ENC_PLATE_T - 0.5, 12.6, 12.6, 5)
    frame = frame - box(-3, 5, ENC_PLATE_T, 6, 4, 5) - box(-3, -9, ENC_PLATE_T, 6, 4, 5)   # Lötfahnen
    return (plate + frame) - union(cut)


# ============================================================================ 4) WAGO-Halter
WAGO415 = (30.0, 8.4, 18.6)   # Breite x Höhe x Tiefe


def wago_halter():
    bw, _bh, bd = WAGO415
    pw, pd, wall, base, wh = bw + 0.4, bd + 0.4, 1.8, 3.0, 6.0
    n = 2
    lx = n * (pw + wall) + wall
    ly = pd + wall
    body = box(0, 0, 0, lx, ly, base + wh)
    cut = []
    for i in range(n):
        x = wall + i * (pw + wall)
        cut.append(box(x, wall, base, pw, pd + 1, wh + 1))           # Tasche, Leiter nach +Y
        cut.append(tie_tunnel(x - 1, ly / 2, 0.8, pw + 2))          # Kabelbinder quer
    add = [body]
    tabs = []
    for tx in (-8.0, lx + 8.0):
        tab, hole = screw_tab(tx, ly / 2, base, w=14, l=16)
        add.append(tab)
        add.append(box(min(tx, 0) if tx < 0 else lx - 1, ly / 2 - 7, 0, 9, 14, base))
        cut.append(hole)
    return union(add) - union(cut)


# ============================================================================ 5) Shelly-Halter
SHELLY = (42.0, 37.0, 16.0)    # Breite x Höhe x Tiefe (Shelly 1 Gen3)


def shelly_halter():
    w, l, _ = SHELLY
    iw, il, wall, base, wh = w + 2 * CLEAR, l + 2 * CLEAR, 2.0, 3.0, 9.0
    lx, ly = iw + 2 * wall, il + wall
    body = box(0, 0, 0, lx, ly, base)
    walls = union([box(0, 0, 0, wall, ly, base + wh), box(lx - wall, 0, 0, wall, ly, base + wh),
                   box(0, ly - wall, 0, lx, wall, base + wh)])   # unten (Y = 0) offen: Klemmen
    add = [body, walls]
    cut = [tie_tunnel(-1, ly * 0.30, 0.8, lx + 2), tie_tunnel(-1, ly * 0.75, 0.8, lx + 2)]
    for tx in (-8.0, lx + 8.0):
        tab, hole = screw_tab(tx, ly / 2, base, w=14, l=16)
        add.append(tab)
        add.append(box(-9 if tx < 0 else lx - 1, ly / 2 - 7, 0, 10, 14, base))
        cut.append(hole)
    return union(add) - union(cut)


# ============================================================================ Export
def mesh_arrays(m):
    mesh = m.to_mesh()
    return mesh.vert_properties[:, :3], mesh.tri_verts


def write_stl(m, path, name):
    v, t = mesh_arrays(m)
    with open(path, "wb") as f:
        f.write(f"CamperUI Radio-Blende {name}, PETG".encode()[:80].ljust(80, b"\0"))
        f.write(struct.pack("<I", len(t)))
        for a, b, c in t:
            p1, p2, p3 = v[a], v[b], v[c]
            ux, uy, uz = p2 - p1
            wx, wy, wz = p3 - p1
            nx, ny, nz = uy * wz - uz * wy, uz * wx - ux * wz, ux * wy - uy * wx
            n = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
            f.write(struct.pack("<12fH", nx / n, ny / n, nz / n, *p1, *p2, *p3, 0))
    return len(t)


def write_3mf(parts, path):
    """Alle Teile als getrennte Objekte in einer 3MF-Datei, nebeneinander angeordnet."""
    objs, items = [], []
    x_off = 0.0
    for i, (name, m) in enumerate(parts.items(), start=1):
        bb = m.bounding_box()
        m2 = m.translate((x_off - bb[0], -bb[1], -bb[2]))
        x_off += bb[3] - bb[0] + 10
        v, t = mesh_arrays(m2)
        vs = "".join(f'<vertex x="{a:.4f}" y="{b:.4f}" z="{c:.4f}"/>' for a, b, c in v)
        ts = "".join(f'<triangle v1="{a}" v2="{b}" v3="{c}"/>' for a, b, c in t)
        objs.append(f'<object id="{i}" name="{name}" type="model"><mesh><vertices>{vs}</vertices>'
                    f'<triangles>{ts}</triangles></mesh></object>')
        items.append(f'<item objectid="{i}"/>')
    model = ('<?xml version="1.0" encoding="UTF-8"?>'
             '<model unit="millimeter" xml:lang="de-DE" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">'
             f'<resources>{"".join(objs)}</resources><build>{"".join(items)}</build></model>')
    ctypes = ('<?xml version="1.0" encoding="UTF-8"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
              '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
              '<Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/></Types>')
    rels = ('<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
            '<Relationship Target="/3D/3dmodel.model" Id="rel0" '
            'Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/></Relationships>')
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", ctypes)
        z.writestr("_rels/.rels", rels)
        z.writestr("3D/3dmodel.model", model)


PARTS = {
    "display_haube": display_haube,
    "pi_gehaeuse": pi_gehaeuse,
    "pi_deckel": pi_deckel,
    "drehgeber_platte": drehgeber_platte,
    "wago_halter": wago_halter,
    "shelly_halter": shelly_halter,
}


def main(out="."):
    os.makedirs(out, exist_ok=True)
    built = {}
    for name, fn in PARTS.items():
        m = fn()
        if m.status() != mf.Error.NoError:
            sys.exit(f"{name}: Geometriefehler {m.status()}")
        bb = m.bounding_box()
        m = m.translate((-bb[0], -bb[1], -bb[2]))
        built[name] = m
        n = write_stl(m, os.path.join(out, name + ".stl"), name)
        bb = m.bounding_box()
        print(f"{name:18s} {n:6d} Dreiecke  {m.volume() / 1000:5.1f} cm³  "
              f"{bb[3] - bb[0]:6.1f} x {bb[4] - bb[1]:6.1f} x {bb[5] - bb[2]:5.1f} mm  "
              f"Körper: {len(m.decompose())}")
    write_3mf(built, os.path.join(out, "radio_blende_teile.3mf"))
    return built


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "out")
