/*
 * Aquarium Cooling Controller - parametric enclosure draft
 *
 * Orientation in this model:
 * - X axis: left/right while looking at the mounted enclosure
 * - Y axis: bottom/top while looking at the mounted enclosure
 * - Z axis: depth/protrusion away from the aquarium rear wall
 *
 * Port layout:
 * - USB-C power input exits downward through the bottom edge
 * - Fan connector/cable exits left
 * - Temperature sensor connector/cables exit right
 *
 * Print/export:
 * - Set part = "base" and export STL for the base
 * - Set part = "lid" and export STL for the lid
 * - part = "assembly" previews both parts and translucent component envelopes
 */

part = "assembly"; // "assembly", "base", "lid"
part_id = 0;        // CLI-friendly: 0 = assembly, 1 = base, 2 = lid

$fn = 36;

// ---------------------------------------------------------------------------
// Main enclosure parameters
// ---------------------------------------------------------------------------

wall = 2.8;              // PETG/PLA friendly wall thickness
floor_thickness = 2.8;
inner_x = 96;            // internal left/right space
inner_y = 78;            // internal bottom/top space
inner_z = 24;            // internal usable height
corner_radius = 4;

outer_x = inner_x + 2 * wall;
outer_y = inner_y + 2 * wall;
outer_z = inner_z + floor_thickness;

lid_thickness = 2.4;
lid_lip_height = 5;
lid_lip_thickness = 1.4;
lid_fit_gap = 0.35;

// M3 self-tapping screws into printed bosses. For threaded inserts, increase
// boss_outer_d and adapt screw_hole_d to the insert maker's recommendation.
screw_hole_d = 2.8;
screw_clearance_d = 3.3;
screw_counterbore_d = 6.2;
boss_outer_d = 8.0;
boss_offset = 7.5;
boss_height = inner_z - lid_lip_height - 0.8;

// Component envelopes. Tune these after measuring the exact modules.
esp32_x = 55;            // ESP32 DevKitC V4, assumed board length
esp32_y = 28;            // ESP32 DevKitC V4, assumed board width
esp32_h = 14;            // board + headers/component allowance

pd_x = 27;               // USB-C PD trigger board
pd_y = 11;
pd_h = 7;

buck_x = 20;             // 12 V -> 5 V buck converter
buck_y = 11;
buck_h = 9;

// JST/cable windows. These are connector-family dependent; measure and tune.
usb_cutout_x = 13.5;
usb_cutout_z = 8.0;
fan_cutout_y = 16;
fan_cutout_z = 9;
temp_cutout_y = 10;
temp_cutout_z = 8;
port_bottom_z = floor_thickness + 4;

// Board positions inside the box, in internal coordinates.
esp32_cx = inner_x / 2;
esp32_cy = 44;
pd_cx = inner_x / 2;
pd_cy = 10;
buck_cx = inner_x / 2;
buck_cy = 70;

fan_port_cy = 44;
temp_port_1_cy = 35;
temp_port_2_cy = 53;
usb_port_cx = pd_cx;

// Rails are used instead of relying on ESP32 mounting holes, which vary
// between DevKit-style boards and clones.
rail_height = 3.2;
rail_width = 2.0;
rail_clearance = 0.7;
end_stop = 2.0;

show_component_envelopes = true;
emboss_lid_labels = true;

// ---------------------------------------------------------------------------
// Utility modules
// ---------------------------------------------------------------------------

module rounded_box(size, r) {
    x = size[0];
    y = size[1];
    z = size[2];
    hull() {
        for (px = [r, x - r])
            for (py = [r, y - r])
                translate([px, py, 0])
                    cylinder(h = z, r = r);
    }
}

module screw_positions() {
    for (p = [
        [wall + boss_offset, wall + boss_offset],
        [outer_x - wall - boss_offset, wall + boss_offset],
        [wall + boss_offset, outer_y - wall - boss_offset],
        [outer_x - wall - boss_offset, outer_y - wall - boss_offset]
    ]) {
        translate([p[0], p[1], 0])
            children();
    }
}

module board_rails(cx, cy, sx, sy) {
    x0 = wall + cx - sx / 2;
    y0 = wall + cy - sy / 2;

    // Side rails along the long direction of the component envelope.
    translate([x0 - rail_width - rail_clearance, y0, floor_thickness])
        cube([rail_width, sy, rail_height]);
    translate([x0 + sx + rail_clearance, y0, floor_thickness])
        cube([rail_width, sy, rail_height]);

    // Small end stops so boards cannot slide out of the rails.
    translate([x0 - rail_width - rail_clearance, y0 - end_stop, floor_thickness])
        cube([sx + 2 * (rail_width + rail_clearance), end_stop, rail_height]);
    translate([x0 - rail_width - rail_clearance, y0 + sy, floor_thickness])
        cube([sx + 2 * (rail_width + rail_clearance), end_stop, rail_height]);
}

module connector_keepout(cx, cy, sx, sy, h) {
    translate([wall + cx - sx / 2, wall + cy - sy / 2, floor_thickness + 0.2])
        cube([sx, sy, h]);
}

module port_windows() {
    // Bottom USB-C opening, aligned with the PD trigger board.
    translate([
        wall + usb_port_cx - usb_cutout_x / 2,
        -1,
        port_bottom_z
    ])
        cube([usb_cutout_x, wall + 2, usb_cutout_z]);

    // Left fan connector/cable opening.
    translate([
        -1,
        wall + fan_port_cy - fan_cutout_y / 2,
        port_bottom_z
    ])
        cube([wall + 2, fan_cutout_y, fan_cutout_z]);

    // Right temperature sensor openings.
    for (cy = [temp_port_1_cy, temp_port_2_cy]) {
        translate([
            outer_x - wall - 1,
            wall + cy - temp_cutout_y / 2,
            port_bottom_z
        ])
            cube([wall + 2, temp_cutout_y, temp_cutout_z]);
    }
}

module base_shell() {
    difference() {
        rounded_box([outer_x, outer_y, outer_z], corner_radius);

        translate([wall, wall, floor_thickness])
            rounded_box(
                [inner_x, inner_y, inner_z + 1],
                max(corner_radius - wall, 0.2)
            );
    }
}

module base() {
    difference() {
        union() {
            base_shell();

            // Lid screw bosses.
            screw_positions()
                translate([0, 0, floor_thickness])
                    cylinder(h = boss_height, d = boss_outer_d);

            // Internal component rails.
            board_rails(esp32_cx, esp32_cy, esp32_x, esp32_y);
            board_rails(pd_cx, pd_cy, pd_x, pd_y);
            board_rails(buck_cx, buck_cy, buck_x, buck_y);

            // Small internal cable-tie/strain-relief saddles near the side exits.
            translate([wall + 5, wall + fan_port_cy - 12, floor_thickness])
                cube([3, 24, 5]);
            translate([outer_x - wall - 8, wall + temp_port_1_cy - 6, floor_thickness])
                cube([3, 12, 5]);
            translate([outer_x - wall - 8, wall + temp_port_2_cy - 6, floor_thickness])
                cube([3, 12, 5]);
        }

        port_windows();

        // Pilot holes in the base bosses.
        screw_positions()
            translate([0, 0, floor_thickness - 0.2])
                cylinder(h = boss_height + 0.5, d = screw_hole_d);

        // Cable-tie slots through the internal saddles.
        translate([wall + 4.5, wall + fan_port_cy - 8, floor_thickness + 1.8])
            cube([4, 16, 2.2]);
        translate([outer_x - wall - 8.5, wall + temp_port_1_cy - 4, floor_thickness + 1.8])
            cube([4, 8, 2.2]);
        translate([outer_x - wall - 8.5, wall + temp_port_2_cy - 4, floor_thickness + 1.8])
            cube([4, 8, 2.2]);
    }
}

module lid_lip() {
    lip_x = inner_x - 2 * lid_fit_gap;
    lip_y = inner_y - 2 * lid_fit_gap;
    inner_lip_x = lip_x - 2 * lid_lip_thickness;
    inner_lip_y = lip_y - 2 * lid_lip_thickness;

    translate([wall + lid_fit_gap, wall + lid_fit_gap, -lid_lip_height])
        difference() {
            rounded_box([lip_x, lip_y, lid_lip_height], max(corner_radius - wall, 0.2));
            translate([lid_lip_thickness, lid_lip_thickness, -0.1])
                rounded_box(
                    [inner_lip_x, inner_lip_y, lid_lip_height + 0.2],
                    max(corner_radius - wall - lid_lip_thickness, 0.2)
                );
        }
}

module lid_labels() {
    if (emboss_lid_labels) {
        linear_extrude(height = 0.55) {
            translate([wall + 6, wall + fan_port_cy - 4])
                rotate([0, 0, 90])
                    text("FAN", size = 5, halign = "center", valign = "center");
            translate([outer_x - wall - 6, wall + 44])
                rotate([0, 0, -90])
                    text("TEMP", size = 5, halign = "center", valign = "center");
            translate([wall + usb_port_cx, wall + 7])
                text("USB-C", size = 5, halign = "center", valign = "center");
        }
    }
}

module lid() {
    difference() {
        union() {
            rounded_box([outer_x, outer_y, lid_thickness], corner_radius);
            lid_lip();
            translate([0, 0, lid_thickness])
                lid_labels();
        }

        // M3 clearance holes through the lid.
        screw_positions()
            translate([0, 0, -lid_lip_height - 0.2])
                cylinder(h = lid_lip_height + lid_thickness + 1, d = screw_clearance_d);

        // Shallow counterbores for screw heads.
        screw_positions()
            translate([0, 0, lid_thickness - 1.2])
                cylinder(h = 1.8, d = screw_counterbore_d);
    }
}

module component_preview() {
    if (show_component_envelopes) {
        color([0.1, 0.35, 0.9, 0.35])
            connector_keepout(esp32_cx, esp32_cy, esp32_x, esp32_y, esp32_h);
        color([0.0, 0.65, 0.25, 0.35])
            connector_keepout(pd_cx, pd_cy, pd_x, pd_y, pd_h);
        color([0.95, 0.55, 0.1, 0.35])
            connector_keepout(buck_cx, buck_cy, buck_x, buck_y, buck_h);
    }
}

// ---------------------------------------------------------------------------
// Render selection
// ---------------------------------------------------------------------------

if (part == "base" || part_id == 1) {
    base();
} else if (part == "lid" || part_id == 2) {
    lid();
} else {
    base();
    component_preview();

    translate([outer_x + 14, 0, lid_lip_height + lid_thickness])
        rotate([180, 0, 0])
            lid();
}
