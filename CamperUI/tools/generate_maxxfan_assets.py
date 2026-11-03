"""Generate matched MaxxFan side-view assets and LVGL 8 RGB565+alpha data."""
from pathlib import Path
from PIL import Image, ImageDraw
ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / 'assets' / 'maxxfan'
OUT.mkdir(parents=True, exist_ok=True)
W, H, SCALE = 140, 54, 4

def build(opened):
    im = Image.new('RGBA', (W*SCALE,H*SCALE))
    d = ImageDraw.Draw(im)
    svg = []
    def poly(points, fill, outline=None, width=1):
        pts=[(round(x*SCALE),round(y*SCALE)) for x,y in points]
        d.polygon(pts,fill=fill)
        if outline:d.line(pts+[pts[0]],fill=outline,width=width*SCALE,joint='curve')
        svg.append(f'<polygon points="{" ".join(f"{x},{y}" for x,y in points)}" fill="{fill}" stroke="{outline or "none"}" stroke-width="{width}" stroke-linejoin="round"/>')
    def line(points,color,width=1):
        pts=[(round(x*SCALE),round(y*SCALE)) for x,y in points]
        d.line(pts,fill=color,width=width*SCALE,joint='curve')
        svg.append(f'<polyline points="{" ".join(f"{x},{y}" for x,y in points)}" fill="none" stroke="{color}" stroke-width="{width}" stroke-linejoin="round"/>')
    edge='#d9e5f0'; base='#526779'; dark='#263e50'
    # Fixed roof flange and raised frame shared by both images.
    poly([(5,47),(9,44),(130,44),(135,47),(135,51),(5,51)],base,edge)
    poly([(16,44),(16,39),(120,39),(120,44)],dark,edge)
    line([(10,48),(130,48)],'#8198ac')
    if opened:
        # Lift support and rear hinge.
        poly([(51,39),(55,39),(68,19),(65,17)],'#869eb3',edge)
        poly([(115,39),(124,31),(128,32),(122,39)],base,edge)
        hood=[(11,7),(21,5),(38,7),(70,14),(104,24),(128,32),(127,37),(107,32),(72,24),(36,15),(14,12)]
        poly(hood,'#526e85',edge,2)
        line([(16,8),(39,10),(73,18),(108,28),(124,33)],'#a4bdd0')
        line([(30,9),(28,15)],'#c1d2df')
        line([(46,13),(44,19)],'#c1d2df')
        line([(62,17),(60,23)],'#c1d2df')
        line([(78,22),(76,27)],'#c1d2df')
    else:
        hood=[(11,33),(17,29),(31,26),(99,26),(118,29),(129,34),(128,39),(13,39)]
        poly(hood,'#526e85',edge,2)
        line([(18,32),(32,29),(98,29),(117,32)],'#a4bdd0')
        for x in [31,43,55,67,79,91]:line([(x,30),(x-2,36)],'#c1d2df')
        poly([(119,40),(125,36),(129,38),(124,42)],base,edge)
    name='open' if opened else 'closed'
    im=im.resize((W,H),Image.Resampling.LANCZOS)
    im.save(OUT/f'maxxfan_{name}.png')
    (OUT/f'maxxfan_{name}.svg').write_text(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}">\n'+ '\n'.join(svg)+'\n</svg>\n')
    return name,im

c=['#include "ui_maxxfan_assets.h"','#if LV_COLOR_DEPTH != 16','#error "MaxxFan assets require 16-bit LVGL colors"','#endif', '#if LV_COLOR_16_SWAP','#define PX(lo, hi, a) hi, lo, a','#else','#define PX(lo, hi, a) lo, hi, a','#endif']
for name,im in [build(False),build(True)]:
    c.append(f'static const uint8_t maxxfan_{name}_pixels[] = {{')
    for y in range(H):
        row=[]
        for x in range(W):
            r,g,b,a=im.getpixel((x,y));rgb=((r>>3)<<11)|((g>>2)<<5)|(b>>3)
            row.append(f'PX(0x{rgb&255:02x},0x{rgb>>8:02x},0x{a:02x})')
        c.append(','.join(row)+',')
    c.append('};')
    c.append(f'const lv_img_dsc_t ui_maxxfan_{name} = {{ .header = {{ .cf = LV_IMG_CF_TRUE_COLOR_ALPHA, .always_zero = 0, .reserved = 0, .w = {W}, .h = {H} }}, .data_size = sizeof(maxxfan_{name}_pixels), .data = maxxfan_{name}_pixels }};')
c.append('#undef PX')
(ROOT/'ui_maxxfan_assets.c').write_text('\n'.join(c)+'\n')
(ROOT/'ui_maxxfan_assets.h').write_text('''#pragma once
#include <lvgl.h>
#ifdef __cplusplus
extern "C" {
#endif
extern const lv_img_dsc_t ui_maxxfan_open;
extern const lv_img_dsc_t ui_maxxfan_closed;
#ifdef __cplusplus
}
#endif
''')
print('Two identical 140 x 54 px assets generated.')
