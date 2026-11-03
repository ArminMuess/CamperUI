#!/usr/bin/env python3
import sys
from pathlib import Path
from PIL import ImageFont, Image

TOOLS_DIR = Path(__file__).resolve().parent
PROJECT_DIR = TOOLS_DIR.parent
TTF_PATH = Path(r"C:\Users\njest\Documents\Arduino\libraries\lvgl\scripts\built_in_font\Montserrat-Medium.ttf")

if not TTF_PATH.exists():
    print(f"Error: {TTF_PATH} not found!")
    sys.exit(1)

CHARS = [
    ('Ä', 0x00C4, 'Adieresis'),
    ('Ö', 0x00D6, 'Odieresis'),
    ('Ü', 0x00DC, 'Udieresis'),
    ('ß', 0x00DF, 'germandbls'),
    ('ä', 0x00E4, 'adieresis'),
    ('ö', 0x00F6, 'odieresis'),
    ('ü', 0x00FC, 'udieresis'),
    ('€', 0x20AC, 'Euro'),
]

FONT_METRICS = {
    10: {'line_height': 11, 'base_line': 2},
    12: {'line_height': 15, 'base_line': 3},
    14: {'line_height': 16, 'base_line': 3},
    16: {'line_height': 18, 'base_line': 3},
    18: {'line_height': 21, 'base_line': 4},
    20: {'line_height': 22, 'base_line': 4},
    22: {'line_height': 24, 'base_line': 4},
    24: {'line_height': 27, 'base_line': 5},
    26: {'line_height': 29, 'base_line': 5},
    28: {'line_height': 30, 'base_line': 5},
    30: {'line_height': 33, 'base_line': 6},
    32: {'line_height': 35, 'base_line': 6},
    42: {'line_height': 46, 'base_line': 8},
}

def generate_font(size):
    font_name = f"ui_font_de_{size}"
    font = ImageFont.truetype(str(TTF_PATH), size)
    ascent, descent = font.getmetrics()
    
    bitmaps = bytearray()
    glyph_dscs = [
        '{.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0}' # id 0 reserved
    ]
    
    for ch, cp, name in CHARS:
        mask = font.getmask(ch)
        bbox = font.getbbox(ch)
        adv_w = int(round(font.getlength(ch) * 16))
        
        box_w, box_h = mask.size
        ofs_x = bbox[0]
        ofs_y = ascent - bbox[3]
        
        bmp_idx = len(bitmaps)
        img = Image.frombytes('L', (box_w, box_h), bytes(mask))
        
        total_bits = box_w * box_h * 4
        num_bytes = (total_bits + 7) // 8
        buf = bytearray(num_bytes)
        
        for y in range(box_h):
            for x in range(box_w):
                p = img.getpixel((x, y))
                v = int(round(p * 15.0 / 255.0)) & 0xF
                bit_ofs = (y * box_w + x) * 4
                byte_idx = bit_ofs // 8
                if bit_ofs % 8 == 0:
                    buf[byte_idx] |= (v << 4)
                else:
                    buf[byte_idx] |= v
                    
        bitmaps.extend(buf)
        glyph_dscs.append(
            f'{{.bitmap_index = {bmp_idx}, .adv_w = {adv_w}, .box_w = {box_w}, .box_h = {box_h}, .ofs_x = {ofs_x}, .ofs_y = {ofs_y}}} /* U+{cp:04X} ({ch}) */'
        )
        
    out = []
    out.append(f'/* =========================================================================')
    out.append(f' * Fallback Font {font_name} ({size}px) for German Umlauts + Euro')
    out.append(f' * ========================================================================= */')
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
    
    out.append(f'static const uint16_t {font_name}_unicodes_0[] = {{')
    rcp_list = [cp - 0x00C4 for _, cp, _ in CHARS[:7]]
    out.append('    ' + ', '.join(str(r) for r in rcp_list))
    out.append('};\n')
    
    out.append(f'static const lv_font_fmt_txt_cmap_t {font_name}_cmaps[] = {{')
    out.append('    {')
    out.append('        .range_start = 0x00C4,')
    out.append('        .range_length = 57,')
    out.append('        .glyph_id_start = 1,')
    out.append(f'        .unicode_list = {font_name}_unicodes_0,')
    out.append('        .glyph_id_ofs_list = NULL,')
    out.append(f'        .list_length = {len(rcp_list)},')
    out.append('        .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY')
    out.append('    },')
    out.append('    {')
    out.append('        .range_start = 0x20AC,')
    out.append('        .range_length = 1,')
    out.append('        .glyph_id_start = 8,')
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
    out.append('    .cmap_num = 2,')
    out.append('    .bpp = 4,')
    out.append('    .kern_classes = 0,')
    out.append('    .bitmap_format = 0,')
    out.append(f'    .cache = &{font_name}_cache')
    out.append('};\n')
    
    met = FONT_METRICS[size]
    out.append(f'const lv_font_t {font_name} = {{')
    out.append('    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,')
    out.append('    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,')
    out.append(f'    .line_height = {met["line_height"]},')
    out.append(f'    .base_line = {met["base_line"]},')
    out.append('    .subpx = LV_FONT_SUBPX_NONE,')
    out.append('    .underline_position = -1,')
    out.append('    .underline_thickness = 1,')
    out.append(f'    .dsc = &{font_name}_font_dsc,')
    out.append('    .fallback = NULL')
    out.append('};\n')
    
    return '\n'.join(out)

def main():
    sizes = [10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 42]
    c_parts = [
        '#include "ui_fonts.h"',
        '#include <lvgl.h>',
        ''
    ]
    
    for s in sizes:
        c_parts.append(generate_font(s))
        
    c_content = '\n'.join(c_parts)
    
    with open(PROJECT_DIR / 'ui_font_de.c', 'w', encoding='utf-8') as f:
        f.write(c_content)
        
    print(f"Generated ui_font_de.c for sizes {sizes} successfully!")

if __name__ == '__main__':
    main()

