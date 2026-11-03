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
    (' ', 0x0020, 'space'),
    ('-', 0x002D, 'minus'),
    ('.', 0x002E, 'period'),
    ('0', 0x0030, 'zero'),
    ('1', 0x0031, 'one'),
    ('2', 0x0032, 'two'),
    ('3', 0x0033, 'three'),
    ('4', 0x0034, 'four'),
    ('5', 0x0035, 'five'),
    ('6', 0x0036, 'six'),
    ('7', 0x0037, 'seven'),
    ('8', 0x0038, 'eight'),
    ('9', 0x0039, 'nine'),
    (':', 0x003A, 'colon'),
]

def generate_clock_font(size=120):
    font_name = f"ui_font_clock_{size}"
    font = ImageFont.truetype(str(TTF_PATH), size)
    ascent, descent = font.getmetrics()
    
    # Scale line height and base line from Montserrat 42: (46, 8)
    line_height = int(round(size * (46.0 / 42.0)))
    base_line = int(round(size * (8.0 / 42.0)))
    
    bitmaps = bytearray()
    glyph_dscs = [
        '{.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */'
    ]
    
    for ch, cp, name in CHARS:
        adv_w = int(round(font.getlength(ch) * 16))
        if ch == ' ':
            glyph_dscs.append(
                f'{{.bitmap_index = {len(bitmaps)}, .adv_w = {adv_w}, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0}} /* U+{cp:04X} ({name}) */'
            )
            continue
            
        mask = font.getmask(ch)
        bbox = font.getbbox(ch)
        m_bbox = mask.getbbox()
        
        if not m_bbox:
            glyph_dscs.append(
                f'{{.bitmap_index = {len(bitmaps)}, .adv_w = {adv_w}, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0}} /* U+{cp:04X} ({name}) */'
            )
            continue
            
        box_w = m_bbox[2] - m_bbox[0]
        box_h = m_bbox[3] - m_bbox[1]
        ofs_x = bbox[0] + m_bbox[0]
        ofs_y = ascent - (bbox[1] + m_bbox[3])
        
        bmp_idx = len(bitmaps)
        
        # Crop to the active ink area
        full_img = Image.frombytes('L', mask.size, bytes(mask))
        cropped = full_img.crop(m_bbox)
        
        # 4bpp packed buffer
        total_bits = box_w * box_h * 4
        num_bytes = (total_bits + 7) // 8
        buf = bytearray(num_bytes)
        
        for y in range(box_h):
            for x in range(box_w):
                p = cropped.getpixel((x, y))
                v = int(round(p * 15.0 / 255.0)) & 0xF
                bit_ofs = (y * box_w + x) * 4
                byte_idx = bit_ofs // 8
                if bit_ofs % 8 == 0:
                    buf[byte_idx] |= (v << 4)
                else:
                    buf[byte_idx] |= v
                    
        bitmaps.extend(buf)
        glyph_dscs.append(
            f'{{.bitmap_index = {bmp_idx}, .adv_w = {adv_w}, .box_w = {box_w}, .box_h = {box_h}, .ofs_x = {ofs_x}, .ofs_y = {ofs_y}}} /* U+{cp:04X} (\'{ch}\') */'
        )
        
    out = []
    out.append('/* =========================================================================')
    out.append(f' * Large Clock Font {font_name} ({size}px) - Montserrat-Medium')
    out.append(' * Characters: [space], -, ., 0-9, :')
    out.append(' * ========================================================================= */')
    out.append('#include "ui_fonts.h"')
    out.append('#include <lvgl.h>\n')
    
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
    rcp_list = [cp - 0x0020 for _, cp, _ in CHARS]
    out.append('    ' + ', '.join(str(r) for r in rcp_list))
    out.append('};\n')
    
    out.append(f'static const lv_font_fmt_txt_cmap_t {font_name}_cmaps[] = {{')
    out.append('    {')
    out.append('        .range_start = 0x0020,')
    out.append(f'        .range_length = {CHARS[-1][1] - 0x0020 + 1},')
    out.append('        .glyph_id_start = 1,')
    out.append(f'        .unicode_list = {font_name}_unicodes_0,')
    out.append('        .glyph_id_ofs_list = NULL,')
    out.append(f'        .list_length = {len(rcp_list)},')
    out.append('        .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY')
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
    out.append(f'    .line_height = {line_height},')
    out.append(f'    .base_line = {base_line},')
    out.append('    .subpx = LV_FONT_SUBPX_NONE,')
    out.append('    .underline_position = -1,')
    out.append('    .underline_thickness = 1,')
    out.append(f'    .dsc = &{font_name}_font_dsc,')
    out.append('    .fallback = NULL')
    out.append('};\n')
    
    return '\n'.join(out)

def main():
    c_content = generate_clock_font(120)
    target = PROJECT_DIR / 'ui_font_clock_120.c'
    with open(target, 'w', encoding='utf-8') as f:
        f.write(c_content)
    print(f"Generated {target} successfully!")

if __name__ == '__main__':
    main()
