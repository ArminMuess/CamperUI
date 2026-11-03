#!/usr/bin/env python3
"""
CamperUI 3D Printable STL Generator
Generates clean binary STL files for Waveshare ESP32-S3-Touch-LCD-4:
  1. flush_mount_frame.stl   (Wandeinbaurahmen - Wandüberstand nur ~3mm)
  2. front_bezel.stl         (Eleganter Abdeck-/Zierrahmen)
  3. surface_mount_case.stl  (Flaches Aufputzgehäuse mit rückseitiger Wandöffnung)
"""

import os
import struct
import math

def write_binary_stl(filepath, triangles):
    """Writes a list of ((x1,y1,z1), (x2,y2,z2), (x3,y3,z3)) to a binary STL file."""
    header = b'CamperUI Waveshare ESP32-S3-Touch-LCD-4 Mount Enclosure (PETG/ABS)'
    header = header.ljust(80, b'\0')
    
    with open(filepath, 'wb') as f:
        f.write(header)
        f.write(struct.pack('<I', len(triangles)))
        for p1, p2, p3 in triangles:
            # Calculate face normal
            ux = p2[0] - p1[0]
            uy = p2[1] - p1[1]
            uz = p2[2] - p1[2]
            vx = p3[0] - p1[0]
            vy = p3[1] - p1[1]
            vz = p3[2] - p1[2]
            nx = uy * vz - uz * vy
            ny = uz * vx - ux * vz
            nz = ux * vy - uy * vx
            length = math.sqrt(nx*nx + ny*ny + nz*nz)
            if length > 1e-9:
                nx /= length
                ny /= length
                nz /= length
            else:
                nx, ny, nz = 0.0, 0.0, 1.0

            f.write(struct.pack('<3f', nx, ny, nz))
            f.write(struct.pack('<3f', *p1))
            f.write(struct.pack('<3f', *p2))
            f.write(struct.pack('<3f', *p3))
            f.write(struct.pack('<H', 0))

def make_box(x1, y1, z1, x2, y2, z2):
    """Returns 12 triangles forming an axis-aligned box."""
    v = [
        (x1, y1, z1), (x2, y1, z1), (x2, y2, z1), (x1, y2, z1),
        (x1, y1, z2), (x2, y1, z2), (x2, y2, z2), (x1, y2, z2)
    ]
    faces = [
        (0, 2, 1), (0, 3, 2), # Bottom -Z
        (4, 5, 6), (4, 6, 7), # Top +Z
        (0, 1, 5), (0, 5, 4), # Front -Y
        (2, 3, 7), (2, 7, 6), # Back +Y
        (0, 4, 7), (0, 7, 3), # Left -X
        (1, 2, 6), (1, 6, 5)  # Right +X
    ]
    tris = []
    for i1, i2, i3 in faces:
        tris.append((v[i1], v[i2], v[i3]))
    return tris

def make_cylinder(cx, cy, z1, z2, radius, segments=24):
    """Returns triangles forming a vertical cylinder."""
    tris = []
    dz = z2 - z1
    for i in range(segments):
        a1 = 2 * math.pi * i / segments
        a2 = 2 * math.pi * (i + 1) / segments
        x1 = cx + radius * math.cos(a1)
        y1 = cy + radius * math.sin(a1)
        x2 = cx + radius * math.cos(a2)
        y2 = cy + radius * math.sin(a2)
        
        # Side quad
        tris.append(((x1, y1, z1), (x2, y2, z1), (x2, y2, z2)))
        tris.append(((x1, y1, z1), (x2, y2, z2), (x1, y1, z2)))
        # Bottom cap
        tris.append(((cx, cy, z1), (x1, y1, z1), (x2, y2, z1)))
        # Top cap
        tris.append(((cx, cy, z2), (x2, y2, z2), (x1, y1, z2)))
    return tris

def make_hollow_box(outer_x1, outer_y1, outer_x2, outer_y2, z1, z2, wall):
    """Returns triangles for 4 perimeter walls."""
    tris = []
    # Left wall
    tris.extend(make_box(outer_x1, outer_y1, z1, outer_x1 + wall, outer_y2, z2))
    # Right wall
    tris.extend(make_box(outer_x2 - wall, outer_y1, z1, outer_x2, outer_y2, z2))
    # Bottom wall (between left and right)
    tris.extend(make_box(outer_x1 + wall, outer_y1, z1, outer_x2 - wall, outer_y1 + wall, z2))
    # Top wall (between left and right)
    tris.extend(make_box(outer_x1 + wall, outer_y2 - wall, z1, outer_x2 - wall, outer_y2, z2))
    return tris

def generate_flush_mount_frame():
    """
    Builds the Flush Mount Frame:
    - Flange resting flat on the wall (94 x 94 x 2.2mm)
    - Pocket box that inserts into the wall cutout (77 x 79 x 13.5mm)
    - 4 Standoff bosses with M2.5 holes (pitch: 66.5 x 68.5mm)
    """
    tris = []
    
    # Outer flange: 94 x 94 x 2.2mm with central window of 74 x 74mm
    fw = 94.0
    fh = 94.0
    flange_t = 2.2
    glass_w = 84.8
    glass_h = 84.8
    
    # Flange perimeter (outside the glass)
    tris.extend(make_hollow_box(0, 0, fw, fh, 0, flange_t, (fw - glass_w) / 2))
    
    # Wall insert body: 77.0 x 79.0 mm extends back from z=0 to z=-13.5mm
    iw = 77.0
    ih = 79.0
    id = 13.5
    ix1 = (fw - iw) / 2
    iy1 = (fh - ih) / 2
    ix2 = ix1 + iw
    iy2 = iy1 + ih
    wall_t = 2.0
    
    # Walls going into the wall cutout
    tris.extend(make_hollow_box(ix1, iy1, ix2, iy2, -id, 0, wall_t))
    
    # 4 Corner Standoff Bosses for Display Screws (pitch 66.5 x 68.5)
    cx = fw / 2
    cy = fh / 2
    h_dx = 66.50 / 2
    h_dy = 68.50 / 2
    for sx in [-1, 1]:
        for sy in [-1, 1]:
            hx = cx + sx * h_dx
            hy = cy + sy * h_dy
            # Outer post Ø 7mm from z=-id+2 to z=0
            tris.extend(make_cylinder(hx, hy, -id + 2.0, 0, 3.5, segments=16))
            
    return tris

def generate_front_bezel():
    """
    Builds the Front Slim Bezel:
    - Ultra-flat snap-on border (90 x 90 x 2.0mm)
    - Window matching active display (72.5 x 72.5mm)
    - Covers glass border and wall cutout gap
    """
    tris = []
    bw = 90.0
    bh = 90.0
    bt = 2.0
    win_w = 72.5
    win_h = 72.5
    wall = (bw - win_w) / 2 # ~8.75mm border
    
    tris.extend(make_hollow_box(0, 0, bw, bh, 0, bt, wall))
    return tris

def generate_surface_mount_case():
    """
    Builds the Surface Mount Case:
    - 89 x 89 x 16.5mm
    - Backplate with large center cable hole (48 x 32mm)
    - 4 Standoffs for M2.5 screws
    """
    tris = []
    cw = 89.0
    ch = 89.0
    cd = 16.5
    wall = 2.2
    
    # Perimeter walls
    tris.extend(make_hollow_box(0, 0, cw, ch, 0, cd, wall))
    
    # Back plate (z=0 to z=wall) with central cable opening
    cable_w = 48.0
    cable_h = 32.0
    cx1 = (cw - cable_w) / 2
    cy1 = (ch - cable_h) / 2
    cx2 = cx1 + cable_w
    cy2 = cy1 + cable_h
    
    # Back plate sections around cable hole
    tris.extend(make_box(0, 0, 0, cw, cy1, wall))             # bottom section
    tris.extend(make_box(0, cy2, 0, cw, ch, wall))             # top section
    tris.extend(make_box(0, cy1, 0, cx1, cy2, wall))           # left section
    tris.extend(make_box(cx2, cy1, 0, cw, cy2, wall))          # right section
    
    # 4 Corner Standoff Bosses for Display Screws (pitch 66.5 x 68.5)
    cx = cw / 2
    cy = ch / 2
    h_dx = 66.50 / 2
    h_dy = 68.50 / 2
    for sx in [-1, 1]:
        for sy in [-1, 1]:
            hx = cx + sx * h_dx
            hy = cy + sy * h_dy
            tris.extend(make_cylinder(hx, hy, wall, wall + 4.0, 3.5, segments=16))
            
    return tris

def main():
    out_dir = os.path.join(os.path.dirname(__file__), '..', 'docs', 'cad')
    os.makedirs(out_dir, exist_ok=True)
    
    print('Generating CamperUI 3D Enclosure STL models...')
    
    # 1. Flush Mount Frame
    f1 = os.path.join(out_dir, 'flush_mount_frame.stl')
    t1 = generate_flush_mount_frame()
    write_binary_stl(f1, t1)
    print(f'Created: {f1} ({len(t1)} triangles)')
    
    # 2. Front Bezel
    f2 = os.path.join(out_dir, 'front_bezel.stl')
    t2 = generate_front_bezel()
    write_binary_stl(f2, t2)
    print(f'Created: {f2} ({len(t2)} triangles)')
    
    # 3. Surface Mount Case
    f3 = os.path.join(out_dir, 'surface_mount_case.stl')
    t3 = generate_surface_mount_case()
    write_binary_stl(f3, t3)
    print(f'Created: {f3} ({len(t3)} triangles)')
    
    print('All STL models successfully generated!')

if __name__ == '__main__':
    main()
