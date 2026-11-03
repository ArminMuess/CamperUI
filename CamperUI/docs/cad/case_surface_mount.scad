// =============================================================================
// CamperUI - Waveshare ESP32-S3-Touch-LCD-4 Surface Mount Enclosure
// Parametric OpenSCAD Model (Flaches Aufputzgehäuse mit Wanddurchführung)
// =============================================================================

$fn = 60;

// --- Dimensions (in mm) ---
glass_w         = 84.20;
glass_h         = 84.20;
glass_t         = 1.50;
tol             = 0.35;

pcb_w           = 74.50;
pcb_h           = 76.50;
total_depth     = 16.50; // Total case depth on the wall

hole_dx         = 66.50; // M2.5 Hole pitch X
hole_dy         = 68.50; // M2.5 Hole pitch Y
screw_d         = 2.80;
standoff_h      = 3.50;

case_w          = 89.00;
case_h          = 89.00;
wall_t          = 2.00;
corner_r        = 4.00;

cable_hole_w    = 48.00; // Hole in the back for wall cable routing
cable_hole_h    = 34.00;

// --- Rounded Box Module ---
module rounded_box(w, h, d, r) {
    linear_extrude(d) {
        hull() {
            translate([r, r, 0]) circle(r=r);
            translate([w - r, r, 0]) circle(r=r);
            translate([w - r, h - r, 0]) circle(r=r);
            translate([r, h - r, 0]) circle(r=r);
        }
    }
}

// =============================================================================
// SURFACE MOUNT CASE BODY
// =============================================================================
module surface_mount_case() {
    difference() {
        // Outer Shell
        rounded_box(case_w, case_h, total_depth, corner_r);
        
        // Front Glass Pocket
        translate([(case_w - (glass_w + tol * 2)) / 2, (case_h - (glass_h + tol * 2)) / 2, total_depth - glass_t])
            cube([glass_w + tol * 2, glass_h + tol * 2, glass_t + 1]);

        // Main Inner Cavity
        translate([(case_w - (pcb_w + tol * 2)) / 2, (case_h - (pcb_h + tol * 2)) / 2, wall_t])
            cube([pcb_w + tol * 2, pcb_h + tol * 2, total_depth]);

        // Rear Wall Cable Cutout (aligns with green terminal block & wall hole)
        translate([(case_w - cable_hole_w) / 2, (case_h - pcb_h) / 2 + 2, -1])
            cube([cable_hole_w, cable_hole_h, wall_t + 2]);

        // Side USB-C Slot (Left)
        translate([-1, case_h / 2 - 8, total_depth - 10])
            cube([wall_t + 2, 16, 7]);

        // Wall Mounting Holes (2x Countersunk holes in the back plate for Spax/Wood screws)
        translate([case_w / 2, case_h - 14, -1]) {
            cylinder(d=3.5, h=wall_t + 2);
            translate([0, 0, 1.2]) cylinder(d1=3.5, d2=7.0, h=wall_t);
        }
        translate([case_w / 2, 14, -1]) {
            cylinder(d=3.5, h=wall_t + 2);
            translate([0, 0, 1.2]) cylinder(d1=3.5, d2=7.0, h=wall_t);
        }
    }

    // 4 Corner Standoffs for Display Screws (M2.5)
    cx = case_w / 2;
    cy = case_h / 2;
    for (sx = [-1, 1]) {
        for (sy = [-1, 1]) {
            hx = cx + sx * (hole_dx / 2);
            hy = cy + sy * (hole_dy / 2);
            translate([hx, hy, wall_t]) {
                difference() {
                    cylinder(d=7.0, h=standoff_h);
                    translate([0, 0, -1]) cylinder(d=screw_d, h=standoff_h + 2);
                }
            }
        }
    }
}

// Render the Surface Mount Case
surface_mount_case();
