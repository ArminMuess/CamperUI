// =============================================================================
// CamperUI - Waveshare ESP32-S3-Touch-LCD-4 Flush Wall Mount Enclosure
// Parametric OpenSCAD Model
// =============================================================================

$fn = 60;

// --- Dimensions (in mm) ---
glass_w         = 84.20; // Glass width
glass_h         = 84.20; // Glass height
glass_t         = 1.50;  // Glass depth/thickness
tol             = 0.35;  // Clearance tolerance for 3D printing

pcb_w           = 74.50; // PCB width
pcb_h           = 76.50; // PCB height
pcb_depth       = 13.50; // Depth of PCB + rear components

hole_dx         = 66.50; // Hole pitch X
hole_dy         = 68.50; // Hole pitch Y
screw_d         = 2.80;  // M2.5 hole (or heat-set insert)
standoff_h      = 4.00;  // Standoff height

flange_w        = 94.00; // Outer flange covering wall cutout
flange_h        = 94.00;
flange_t        = 2.20;  // Flange thickness (wall protrusion)
corner_r        = 4.00;

cutout_w        = 76.80; // Insert box fitting into wall hole
cutout_h        = 78.80;
wall_t          = 1.80;

terminal_w      = 46.00; // Cutout for green terminal block
terminal_h      = 20.00;

// --- Rounded Rectangle Helper ---
module rounded_rect(w, h, t, r) {
    linear_extrude(t) {
        hull() {
            translate([r, r, 0]) circle(r=r);
            translate([w - r, r, 0]) circle(r=r);
            translate([w - r, h - r, 0]) circle(r=r);
            translate([r, h - r, 0]) circle(r=r);
        }
    }
}

// =============================================================================
// 1. FLUSH MOUNT FRAME (Wandeinbaurahmen)
// =============================================================================
module flush_mount_frame() {
    difference() {
        union() {
            // A) Outer Face Flange
            rounded_rect(flange_w, flange_h, flange_t, corner_r);
            
            // B) Wall Pocket Box (extends into wall)
            translate([(flange_w - cutout_w) / 2, (flange_h - cutout_h) / 2, 0])
                cube([cutout_w, cutout_h, pcb_depth + flange_t]);
        }
        
        // C) Recess for Glass Front (bündig im Rahmen)
        translate([(flange_w - (glass_w + tol * 2)) / 2, (flange_h - (glass_h + tol * 2)) / 2, flange_t - glass_t])
            cube([glass_w + tol * 2, glass_h + tol * 2, glass_t + 1]);

        // D) Inner Cavity for PCB
        translate([(flange_w - (pcb_w + tol * 2)) / 2, (flange_h - (pcb_h + tol * 2)) / 2, -1])
            cube([pcb_w + tol * 2, pcb_h + tol * 2, pcb_depth + flange_t + 2]);

        // E) Rear Terminal Block & Cable Passage Cutout
        translate([(flange_w - terminal_w) / 2, (flange_h - cutout_h) / 2 - 1, pcb_depth - 4])
            cube([terminal_w, terminal_h + 2, 20]);

        // F) Wall Mounting Screw Holes (4x Countersunk 3mm in flange corners)
        screw_inset = 4.2;
        for (pos = [
            [screw_inset, screw_inset],
            [flange_w - screw_inset, screw_inset],
            [flange_w - screw_inset, flange_h - screw_inset],
            [screw_inset, flange_h - screw_inset]
        ]) {
            translate([pos[0], pos[1], -1]) {
                cylinder(d=3.2, h=flange_t + 2);
                translate([0, 0, flange_t - 1.2]) cylinder(d1=3.2, d2=6.0, h=1.5);
            }
        }
    }

    // G) 4 Mounting Lugs for Display M2.5 Screws
    cx = flange_w / 2;
    cy = flange_h / 2;
    for (sx = [-1, 1]) {
        for (sy = [-1, 1]) {
            hx = cx + sx * (hole_dx / 2);
            hy = cy + sy * (hole_dy / 2);
            translate([hx, hy, flange_t]) {
                difference() {
                    cylinder(d=7.0, h=standoff_h);
                    translate([0, 0, -1]) cylinder(d=screw_d, h=standoff_h + 2);
                }
            }
        }
    }
}

// Render the Flush Mount Frame
flush_mount_frame();
