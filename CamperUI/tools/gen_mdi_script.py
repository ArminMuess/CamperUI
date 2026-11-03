from PIL import Image, ImageFont, ImageDraw
from pathlib import Path

TOOLS_DIR = Path(__file__).resolve().parent
PROJECT_DIR = TOOLS_DIR.parent

icons = [
    ('MDI_LIGHTBULB',        0xf0335, 'lightbulb'),
    ('MDI_BATTERY_CHARGING', 0xf0082, 'battery-charging'),
    ('MDI_WATER',            0xf058c, 'water'),
    ('MDI_THERMOMETER',      0xf050f, 'thermometer'),
    ('MDI_TOGGLE_SWITCH',    0xf0521, 'toggle-switch'),
    ('MDI_SPIRIT_LEVEL',     0xf14f1, 'spirit-level'),
    ('MDI_TUNE',             0xf062e, 'tune'),
    ('MDI_WIFI',             0xf05a9, 'wifi'),
    ('MDI_BATTERY',          0xf0079, 'battery'),
    ('MDI_PUMP',             0xf058f, 'water-pump'),
    ('MDI_FIRE',             0xf0238, 'fire'),
    ('MDI_SNOWFLAKE',        0xf0717, 'snowflake'),
    ('MDI_ALERT',            0xf0028, 'alert-circle'),
    ('MDI_SOLAR',            0xf0a72, 'solar-power'),
    ('MDI_CHECK',            0xf012c, 'check-bold'),
    ('MDI_HOME',             0xf02dc, 'home'),
    ('MDI_FAN',              0xf0210, 'fan'),
    ('MDI_COG',              0xf0493, 'cog'),
    ('MDI_EYE',              0xf0208, 'eye'),
    ('MDI_SLIDERS',          0xf0a49, 'sliders'),
    ('MDI_BELL',             0xf009a, 'bell'),
    ('MDI_CLOCK',            0xf0150, 'clock'),
    ('MDI_LOCK',             0xf033e, 'lock'),
    ('MDI_STETHOSCOPE',      0xf04d9, 'stethoscope'),
]

def generate_font_c(font_name, size, ttf_path):
    font = ImageFont.truetype(ttf_path, size)
    
    bitmaps = bytearray()
    glyph_dscs = [
        '{.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0}' # id 0
    ]
    
    for name, cp, desc in icons:
        ch = chr(cp)
        bbox = font.getbbox(ch)
        bw = bbox[2] - bbox[0]
        bh = bbox[3] - bbox[1]
        ox = (size - bw) // 2 - bbox[0]
        oy = (size - bh) // 2 - bbox[1]
        
        img = Image.new('L', (size, size), 0)
        draw = ImageDraw.Draw(img)
        draw.text((ox, oy), ch, font=font, fill=255)
        
        bmp_idx = len(bitmaps)
        total_bits = size * size * 4
        num_bytes = (total_bits + 7) // 8
        buf = bytearray(num_bytes)
        
        for y in range(size):
            for x in range(size):
                p = img.getpixel((x, y))
                v = int(round(p * 15.0 / 255.0)) & 0xF
                bit_ofs = (y * size + x) * 4
                byte_idx = bit_ofs // 8
                if bit_ofs % 8 == 0:
                    buf[byte_idx] |= (v << 4)
                else:
                    buf[byte_idx] |= v
                    
        bitmaps.extend(buf)
        glyph_dscs.append(
            f'{{.bitmap_index = {bmp_idx}, .adv_w = {size * 16}, .box_w = {size}, .box_h = {size}, .ofs_x = 0, .ofs_y = 0}} /* {desc} */'
        )
        
    out = []
    out.append(f'/* Auto-generated {font_name} ({size}px) */')
    out.append(f'static const uint8_t {font_name}_bitmap[] = {{')
    line = '    '
    for i, b in enumerate(bitmaps):
        line += f'0x{b:02x}, '
        if (i + 1) % 16 == 0:
            out.append(line)
            line = '    '
    if line.strip():
        out.append(line)
    out.append('};\n')
    
    out.append(f'static const lv_font_fmt_txt_glyph_dsc_t {font_name}_glyph_dsc[] = {{')
    for d in glyph_dscs:
        out.append(f'    {d},')
    out.append('};\n')
    
    out.append(f'static const lv_font_fmt_txt_cmap_t {font_name}_cmaps[] = {{')
    out.append('    {')
    out.append('        .range_start = 0xE001,')
    out.append(f'        .range_length = {len(icons)},')
    out.append('        .glyph_id_start = 1,')
    out.append('        .unicode_list = NULL,')
    out.append('        .glyph_id_ofs_list = NULL,')
    out.append('        .list_length = 0,')
    out.append('        .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY')
    out.append('    }')
    out.append('};\n')
    
    out.append(f'static lv_font_fmt_txt_glyph_cache_t {font_name}_cache;')
    out.append(f'static const lv_font_fmt_txt_dsc_t {font_name}_font_dsc = {{')
    out.append(f'    .glyph_bitmap = {font_name}_bitmap,')
    out.append(f'    .glyph_dsc = {font_name}_glyph_dsc,')
    out.append(f'    .cmaps = {font_name}_cmaps,')
    out.append('    .kern_dsc = NULL,')
    out.append('    .kern_scale = 0,')
    out.append('    .cmap_num = 1,')
    out.append('    .bpp = 4,')
    out.append('    .kern_classes = 0,')
    out.append('    .bitmap_format = 0,')
    out.append(f'    .cache = &{font_name}_cache')
    out.append('};\n')
    
    out.append(f'const lv_font_t {font_name} = {{')
    out.append('    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,')
    out.append('    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,')
    out.append(f'    .line_height = {size},')
    out.append('    .base_line = 0,')
    out.append('    .subpx = LV_FONT_SUBPX_NONE,')
    out.append('    .underline_position = 0,')
    out.append('    .underline_thickness = 0,')
    out.append(f'    .dsc = &{font_name}_font_dsc')
    out.append('};\n')
    
    return '\n'.join(out)

c_header = '#include "ui_mdi_icons.h"\n#include <lvgl.h>\n\n'
c_content = c_header + \
    generate_font_c('ui_font_mdi_32', 32, str(TOOLS_DIR / 'materialdesignicons-webfont.ttf')) + '\n' + \
    generate_font_c('ui_font_mdi_24', 24, str(TOOLS_DIR / 'materialdesignicons-webfont.ttf')) + '\n' + \
    generate_font_c('ui_font_mdi_18', 18, str(TOOLS_DIR / 'materialdesignicons-webfont.ttf'))

with open(PROJECT_DIR / 'ui_mdi_icons.c', 'w', encoding='utf-8') as f:
    f.write(c_content)

h_content = """#ifndef UI_MDI_ICONS_H
#define UI_MDI_ICONS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Material Design Icons Unicode PUA Mappings (0xE001 .. 0xE018)
#define MDI_LIGHTBULB          "\\xEE\\x80\\x81" // 0xE001: Dimmer / Licht
#define MDI_BATTERY_CHARGING   "\\xEE\\x80\\x82" // 0xE002: Power
#define MDI_WATER              "\\xEE\\x80\\x83" // 0xE003: Wasser
#define MDI_THERMOMETER        "\\xEE\\x80\\x84" // 0xE004: Klima
#define MDI_TOGGLE_SWITCH      "\\xEE\\x80\\x85" // 0xE005: Relais
#define MDI_SPIRIT_LEVEL       "\\xEE\\x80\\x86" // 0xE006: Wasserwaage
#define MDI_TUNE               "\\xEE\\x80\\x87" // 0xE007: Setup / Einstellungen
#define MDI_WIFI               "\\xEE\\x80\\x88" // 0xE008: Header WiFi
#define MDI_BATTERY            "\\xEE\\x80\\x89" // 0xE009: Header Battery
#define MDI_PUMP               "\\xEE\\x80\\x8A" // 0xE00A: Header Pump
#define MDI_FIRE               "\\xEE\\x80\\x8B" // 0xE00B: Header Heater / Fire
#define MDI_SNOWFLAKE          "\\xEE\\x80\\x8C" // 0xE00C: Header Frost
#define MDI_ALERT              "\\xEE\\x80\\x8D" // 0xE00D: Alert
#define MDI_SOLAR              "\\xEE\\x80\\x8E" // 0xE00E: Solar Power
#define MDI_CHECK              "\\xEE\\x80\\x8F" // 0xE00F: Check OK

#define MDI_HOME               "\\xEE\\x80\\x90" // 0xE010: Home

#define MDI_FAN                "\\xEE\\x80\\x91" // 0xE011: MaxxFan

#define MDI_COG                "\\xEE\\x80\\x92" // 0xE012: Settings / Allgemein
#define MDI_EYE                "\\xEE\\x80\\x93" // 0xE013: Anzeige / Display
#define MDI_SLIDERS            "\\xEE\\x80\\x94" // 0xE014: Werte / Parameter
#define MDI_BELL               "\\xEE\\x80\\x95" // 0xE015: Alarme
#define MDI_CLOCK              "\\xEE\\x80\\x96" // 0xE016: Zeit / Uhr
#define MDI_LOCK               "\\xEE\\x80\\x97" // 0xE017: Lock / Sperre
#define MDI_STETHOSCOPE               "\\xEE\\x80\\x98" // 0xE018: Stethoskop / Diagnose

LV_FONT_DECLARE(ui_font_mdi_32);
LV_FONT_DECLARE(ui_font_mdi_24);
LV_FONT_DECLARE(ui_font_mdi_18);

#ifdef __cplusplus
}
#endif

#endif // UI_MDI_ICONS_H
"""

with open(PROJECT_DIR / 'ui_mdi_icons.h', 'w', encoding='utf-8') as f:
    f.write(h_content)

print('Generated ui_mdi_icons.c and ui_mdi_icons.h successfully!')

