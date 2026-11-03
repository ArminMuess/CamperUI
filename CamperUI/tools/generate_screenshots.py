import os
import math
from PIL import Image, ImageDraw, ImageFont

BG_COLOR = (20, 23, 29)       # 0x14171d
CARD_COLOR = (30, 35, 43)     # 0x1e232b
BORDER_COLOR = (46, 53, 69)   # 0x2e3545
TRACK_COLOR = (18, 21, 28)    # 0x12151c
TEXT_COLOR = (248, 250, 252)  # 0xf8fafc
MUTED_COLOR = (148, 163, 184) # 0x94a3b8
PRIMARY = (14, 165, 233)      # 0x0ea5e9
SUCCESS = (16, 185, 129)      # 0x10b981
WARNING = (245, 158, 11)      # 0xf59e0b
DANGER = (239, 68, 68)        # 0xef4444

MDI_FONT_PATH = 'tools/materialdesignicons-webfont.ttf'
FONT_REG_PATH = 'C:/Windows/Fonts/segoeui.ttf'
FONT_BOLD_PATH = 'C:/Windows/Fonts/segoeuib.ttf'

font_mdi_32 = ImageFont.truetype(MDI_FONT_PATH, 32)
font_mdi_18 = ImageFont.truetype(MDI_FONT_PATH, 18)
font_12 = ImageFont.truetype(FONT_REG_PATH, 12)
font_14 = ImageFont.truetype(FONT_REG_PATH, 14)
font_16 = ImageFont.truetype(FONT_REG_PATH, 16)
font_16b = ImageFont.truetype(FONT_BOLD_PATH, 16)
font_18 = ImageFont.truetype(FONT_BOLD_PATH, 18)
font_20 = ImageFont.truetype(FONT_BOLD_PATH, 20)
font_28 = ImageFont.truetype(FONT_BOLD_PATH, 28)
font_42 = ImageFont.truetype(FONT_BOLD_PATH, 42)

# MDI glyph code points
MDI_ICONS = {
    'lightbulb': chr(0xf0335),
    'battery_charging': chr(0xf0082),
    'water': chr(0xf058c),
    'thermometer': chr(0xf050f),
    'toggle_switch': chr(0xf0521),
    'spirit_level': chr(0xf14f1),
    'tune': chr(0xf062e),
    'wifi': chr(0xf05a9),
    'battery': chr(0xf0079),
    'pump': chr(0xf058f),
    'fire': chr(0xf0238),
    'snowflake': chr(0xf0717),
    'alert': chr(0xf0028),
    'solar': chr(0xf0a72),
    'check': chr(0xf012c),
}

TAB_ICONS = ['lightbulb', 'battery_charging', 'water', 'thermometer', 'toggle_switch', 'spirit_level', 'tune']

def create_base(active_tab=0):
    img = Image.new('RGB', (480, 480), BG_COLOR)
    draw = ImageDraw.Draw(img)
    
    # 1. Status Bar (40px)
    draw.rectangle([0, 0, 480, 40], fill=(24, 28, 35))
    draw.line([0, 39, 480, 39], fill=BORDER_COLOR)
    
    # Time
    draw.text((16, 10), '12:45', font=font_16b, fill=TEXT_COLOR)
    
    # Badges container
    draw.rounded_rectangle([72, 8, 98, 32], radius=4, fill=(18, 46, 77))
    draw.text((76, 10), MDI_ICONS['water'], font=font_mdi_18, fill=PRIMARY)
    
    # Right status bar items
    draw.text((370, 10), MDI_ICONS['pump'], font=font_mdi_18, fill=PRIMARY)
    draw.text((394, 10), MDI_ICONS['battery'], font=font_mdi_18, fill=SUCCESS)
    draw.text((414, 11), '88%', font=font_14, fill=TEXT_COLOR)
    draw.text((448, 10), MDI_ICONS['wifi'], font=font_mdi_18, fill=SUCCESS)
    
    # 2. Bottom Tab Bar (60px: 420 to 480)
    draw.rectangle([0, 420, 480, 480], fill=(24, 28, 35))
    draw.line([0, 420, 480, 420], fill=BORDER_COLOR)
    
    tab_w = 480 / 7
    for idx, icon_name in enumerate(TAB_ICONS):
        cx = int(idx * tab_w + tab_w / 2)
        color = PRIMARY if idx == active_tab else MUTED_COLOR
        char = MDI_ICONS[icon_name]
        bbox = font_mdi_32.getbbox(char)
        iw = bbox[2] - bbox[0]
        ih = bbox[3] - bbox[1]
        draw.text((cx - iw // 2, 450 - ih // 2), char, font=font_mdi_32, fill=color)
        if idx == active_tab:
            draw.line([int(idx * tab_w + 14), 421, int((idx + 1) * tab_w - 14), 421], fill=PRIMARY, width=3)
            
    return img, draw

# ==========================================
# 1. Dimmers Tab
# ==========================================
def render_dimmers():
    img, draw = create_base(active_tab=0)
    dimmers_data = [
        ('Kueche', 80),
        ('Ambiente', 50),
        ('Schlafzimmer', 30),
        ('Aussenlicht', 0),
    ]
    
    top_y = 50
    card_h = 80
    gap = 10
    
    for i, (name, val) in enumerate(dimmers_data):
        y1 = top_y + i * (card_h + gap)
        y2 = y1 + card_h
        draw.rounded_rectangle([20, y1, 460, y2], radius=14, fill=CARD_COLOR, outline=BORDER_COLOR, width=1)
        
        # Name
        draw.text((36, y1 + 28), name, font=font_18, fill=TEXT_COLOR)
        
        # Slider track: x 150 to 395 (width 245), height 32
        sy1 = y1 + 24
        sy2 = sy1 + 32
        draw.rounded_rectangle([150, sy1, 395, sy2], radius=16, fill=TRACK_COLOR, outline=BORDER_COLOR, width=1)
        
        # Indicator
        if val > 0:
            fill_w = int(245 * val / 100)
            draw.rounded_rectangle([150, sy1, 150 + fill_w, sy2], radius=16, fill=WARNING)
            
        # Percentage label
        pct_text = f'{val}%' if val > 0 else 'Aus'
        pct_color = WARNING if val > 0 else MUTED_COLOR
        draw.text((410, y1 + 28), pct_text, font=font_18, fill=pct_color)
        
    img.save('docs/screenshots/tab_dimmers.png')

# ==========================================
# 2. Power Tab
# ==========================================
def render_power():
    img, draw = create_base(active_tab=1)
    
    # Top Card: Battery
    draw.rounded_rectangle([20, 50, 460, 220], radius=14, fill=CARD_COLOR, outline=BORDER_COLOR, width=1)
    draw.text((36, 62), 'Bordbatterie (LiFePO4)', font=font_16, fill=MUTED_COLOR)
    draw.text((36, 92), '88%', font=font_42, fill=SUCCESS)
    draw.text((140, 110), '13.40 V  |  -2.1 A', font=font_18, fill=TEXT_COLOR)
    draw.text((36, 150), 'Restlaufzeit: ca. 38h  |  Rest: 88.0 Ah / 100 Ah', font=font_14, fill=MUTED_COLOR)
    
    # Bottom Card: Energy Flow
    draw.rounded_rectangle([20, 230, 460, 410], radius=14, fill=CARD_COLOR, outline=BORDER_COLOR, width=1)
    draw.text((36, 242), 'Energiefluss & Solar', font=font_16, fill=MUTED_COLOR)
    
    # Solar box
    draw.rounded_rectangle([36, 280, 160, 390], radius=10, fill=TRACK_COLOR, outline=BORDER_COLOR, width=1)
    draw.text((50, 290), MDI_ICONS['solar'], font=font_mdi_32, fill=WARNING)
    draw.text((50, 330), 'Solar', font=font_14, fill=MUTED_COLOR)
    draw.text((50, 352), '148 W', font=font_18, fill=WARNING)
    
    # Flow arrow
    draw.text((180, 330), '>>>', font=font_20, fill=SUCCESS)
    
    # Battery Box
    draw.rounded_rectangle([220, 280, 320, 390], radius=10, fill=TRACK_COLOR, outline=BORDER_COLOR, width=1)
    draw.text((236, 290), MDI_ICONS['battery_charging'], font=font_mdi_32, fill=SUCCESS)
    draw.text((236, 330), 'Batt', font=font_14, fill=MUTED_COLOR)
    draw.text((236, 352), '+120 W', font=font_18, fill=SUCCESS)
    
    # Flow arrow
    draw.text((340, 330), '>>>', font=font_20, fill=PRIMARY)
    
    # Consumer Box
    draw.rounded_rectangle([380, 280, 444, 390], radius=10, fill=TRACK_COLOR, outline=BORDER_COLOR, width=1)
    draw.text((392, 290), MDI_ICONS['lightbulb'], font=font_mdi_32, fill=PRIMARY)
    draw.text((392, 330), 'Last', font=font_14, fill=MUTED_COLOR)
    draw.text((392, 352), '28 W', font=font_18, fill=PRIMARY)
    
    img.save('docs/screenshots/tab_power.png')

# ==========================================
# 3. Water Tab
# ==========================================
def render_water():
    img, draw = create_base(active_tab=2)
    
    # Top Card: Tanks (height 284)
    draw.rounded_rectangle([20, 50, 460, 334], radius=14, fill=CARD_COLOR, outline=BORDER_COLOR, width=1)
    draw.text((36, 60), 'Wassertanks', font=font_16, fill=MUTED_COLOR)
    
    # Frischwasser (Col 1)
    draw.text((80, 78), 'Frischwasser', font=font_16b, fill=TEXT_COLOR)
    draw.rounded_rectangle([92, 102, 172, 262], radius=12, fill=TRACK_COLOR, outline=BORDER_COLOR, width=1)
    # Fill 75%
    draw.rounded_rectangle([92, 142, 172, 262], radius=12, fill=PRIMARY)
    draw.text((114, 268), '75%', font=font_18, fill=TEXT_COLOR)
    draw.text((106, 294), '68 / 90 L', font=font_14, fill=MUTED_COLOR)
    
    # Grauwasser (Col 2)
    draw.text((300, 78), 'Grauwasser', font=font_16b, fill=TEXT_COLOR)
    draw.rounded_rectangle([308, 102, 388, 262], radius=12, fill=TRACK_COLOR, outline=BORDER_COLOR, width=1)
    # Fill 25%
    draw.rounded_rectangle([308, 222, 388, 262], radius=12, fill=(100, 116, 139))
    draw.text((330, 268), '25%', font=font_18, fill=TEXT_COLOR)
    draw.text((324, 294), '18 / 70 L', font=font_14, fill=MUTED_COLOR)
    
    # Bottom Buttons (Height 64)
    draw.rounded_rectangle([20, 346, 234, 410], radius=14, fill=CARD_COLOR, outline=BORDER_COLOR, width=1)
    draw.text((64, 368), 'Wasserpumpe', font=font_16b, fill=TEXT_COLOR)
    
    draw.rounded_rectangle([246, 346, 460, 410], radius=14, fill=CARD_COLOR, outline=BORDER_COLOR, width=1)
    draw.text((308, 368), 'Abwasserventil', font=font_16b, fill=TEXT_COLOR)
    
    img.save('docs/screenshots/tab_water.png')

# ==========================================
# 4. Climate Tab
# ==========================================
def render_climate():
    img, draw = create_base(active_tab=3)
    
    # Top Left: Outdoor Temp
    draw.rounded_rectangle([20, 50, 170, 108], radius=14, fill=CARD_COLOR, outline=BORDER_COLOR, width=1)
    draw.text((32, 58), 'Aussen', font=font_12, fill=MUTED_COLOR)
    draw.text((32, 74), '14.6 C', font=font_20, fill=TEXT_COLOR)
    
    # Top Right: Mode Dropdown
    draw.rounded_rectangle([270, 50, 460, 108], radius=14, fill=CARD_COLOR, outline=BORDER_COLOR, width=1)
    draw.text((284, 70), 'Heizen (Temp)', font=font_16b, fill=TEXT_COLOR)
    draw.text((430, 70), 'v', font=font_16b, fill=MUTED_COLOR)
    
    # Thermostat Arc (Center cx=240, cy=220, r=105)
    cx, cy, r = 240, 225, 95
    # Draw arc track
    draw.arc([cx - r, cy - r, cx + r, cy - r + 2*r], start=135, end=45, fill=TRACK_COLOR, width=16)
    draw.arc([cx - r, cy - r, cx + r, cy - r + 2*r], start=135, end=330, fill=DANGER, width=16)
    
    # Knob with white background and red border
    angle_rad = math.radians(330)
    kx = cx + r * math.cos(angle_rad)
    ky = cy + r * math.sin(angle_rad)
    draw.ellipse([kx - 14, ky - 14, kx + 14, ky + 14], fill=(255, 255, 255), outline=DANGER, width=4)
    
    # Inside Arc: Indoor & Target
    draw.text((cx - 64, cy - 26), '21.5 C', font=font_42, fill=TEXT_COLOR)
    draw.text((cx - 36, cy + 24), 'Ziel: 22 C', font=font_18, fill=MUTED_COLOR)
    
    # Start / Stop Button
    draw.rounded_rectangle([110, 356, 370, 410], radius=14, fill=SUCCESS)
    draw.text((220, 370), 'Start', font=font_20, fill=(255, 255, 255))
    
    img.save('docs/screenshots/tab_climate.png')

# ==========================================
# 5. Level Tab
# ==========================================
def render_level():
    img, draw = create_base(active_tab=5)
    
    draw.rounded_rectangle([20, 50, 460, 410], radius=14, fill=CARD_COLOR, outline=BORDER_COLOR, width=1)
    draw.text((36, 62), 'Wasserwaage & Keil-Assistent', font=font_18, fill=TEXT_COLOR)
    
    # Circular 2D Bubble Level
    bx, by, br = 118, 176, 75
    draw.ellipse([bx - br, by - br, bx + br, by + br], fill=TRACK_COLOR, outline=BORDER_COLOR, width=2)
    # Crosshair
    draw.line([bx - br, by, bx + br, by], fill=BORDER_COLOR, width=1)
    draw.line([bx, by - br, bx, by + br], fill=BORDER_COLOR, width=1)
    # Target ring
    draw.ellipse([bx - 22, by - 22, bx + 22, by + 22], outline=SUCCESS, width=2)
    # Bubble
    draw.ellipse([bx - 8, by - 24, bx + 20, by + 4], fill=PRIMARY, outline=(255, 255, 255), width=2)
    draw.text((46, 264), 'Ausrichten noetig', font=font_14, fill=WARNING)
    
    # Right: Pitch & Roll
    draw.text((226, 96), 'Neigung Laengs:', font=font_14, fill=MUTED_COLOR)
    draw.text((350, 94), '+1.4 deg', font=font_16b, fill=TEXT_COLOR)
    draw.text((226, 120), 'Neigung Quer:', font=font_14, fill=MUTED_COLOR)
    draw.text((350, 118), '-0.8 deg', font=font_16b, fill=TEXT_COLOR)
    
    # 4-Wheel Box
    draw.rounded_rectangle([226, 150, 430, 290], radius=10, fill=TRACK_COLOR, outline=BORDER_COLOR, width=1)
    
    # 4 Wheel boxes (90x58)
    # Vorne L
    draw.rounded_rectangle([234, 158, 324, 216], radius=8, fill=CARD_COLOR, outline=BORDER_COLOR, width=1)
    draw.text((254, 164), 'Vorne L', font=font_12, fill=MUTED_COLOR)
    draw.text((264, 186), 'OK', font=font_16b, fill=SUCCESS)
    
    # Vorne R
    draw.rounded_rectangle([332, 158, 422, 216], radius=8, fill=CARD_COLOR, outline=BORDER_COLOR, width=1)
    draw.text((352, 164), 'Vorne R', font=font_12, fill=MUTED_COLOR)
    draw.text((352, 186), '+2 cm', font=font_16b, fill=WARNING)
    
    # Hinten L
    draw.rounded_rectangle([234, 224, 324, 282], radius=8, fill=CARD_COLOR, outline=BORDER_COLOR, width=1)
    draw.text((252, 230), 'Hinten L', font=font_12, fill=MUTED_COLOR)
    draw.text((252, 252), '+3 cm', font=font_16b, fill=WARNING)
    
    # Hinten R
    draw.rounded_rectangle([332, 224, 422, 282], radius=8, fill=CARD_COLOR, outline=BORDER_COLOR, width=1)
    draw.text((350, 230), 'Hinten R', font=font_12, fill=MUTED_COLOR)
    draw.text((350, 252), '+5 cm', font=font_16b, fill=WARNING)
    
    # Tara Button
    draw.rounded_rectangle([28, 350, 238, 394], radius=8, fill=PRIMARY)
    draw.text((70, 362), 'Nullpunkt Tara', font=font_14, fill=(255, 255, 255))
    
    img.save('docs/screenshots/tab_level.png')

render_dimmers()
render_power()
render_water()
render_climate()
render_level()
print('All 5 screenshots generated successfully in docs/screenshots/!')

# ==========================================
# 6. Switches Tab
# ==========================================
def render_switches():
    img, draw = create_base(active_tab=4)
    switches_data = [
        ('Kuehlschrank', True),
        ('Wasserpumpe', True),
        ('Aussenlicht', False),
        ('Inverter 230V', True),
        ('USB Ladeports', True),
        ('Standheizung', False),
    ]
    
    col_w = 212
    card_h = 94
    row_gap = 12
    col_gap = 16
    start_x = 20
    start_y = 52
    
    for i, (name, on) in enumerate(switches_data):
        row = i // 2
        col = i % 2
        x1 = start_x + col * (col_w + col_gap)
        y1 = start_y + row * (card_h + row_gap)
        x2 = x1 + col_w
        y2 = y1 + card_h
        
        bg = SUCCESS if on else CARD_COLOR
        border = (52, 211, 153) if on else BORDER_COLOR
        txt_c = (255, 255, 255) if on else TEXT_COLOR
        
        draw.rounded_rectangle([x1, y1, x2, y2], radius=14, fill=bg, outline=border, width=1)
        draw.text((x1 + 30, y1 + 36), name, font=font_18, fill=txt_c)
        
    img.save('docs/screenshots/tab_switches.png')

# ==========================================
# 7. Settings Tab
# ==========================================
def render_settings():
    img, draw = create_base(active_tab=6)
    
    draw.rounded_rectangle([20, 50, 460, 410], radius=14, fill=CARD_COLOR, outline=BORDER_COLOR, width=1)
    draw.text((36, 62), 'System Einstellungen', font=font_18, fill=TEXT_COLOR)
    
    items = [
        ('WLAN SSID', 'CamperConnect_5G'),
        ('VanPi IP-Adresse', '192.168.4.1'),
        ('Display Helligkeit', '85%'),
        ('Dark Mode', 'Aktiv'),
        ('Alarmschwelle Frost', '< 3.0 C'),
        ('Alarmschwelle Batterie', '< 20%'),
        ('Debug-Simulation', 'Inaktiv'),
    ]
    
    sy = 96
    for title, val in items:
        draw.line([36, sy - 4, 444, sy - 4], fill=BORDER_COLOR, width=1)
        draw.text((36, sy + 4), title, font=font_14, fill=MUTED_COLOR)
        draw.text((310, sy + 4), val, font=font_16b, fill=TEXT_COLOR)
        sy += 40
        
    img.save('docs/screenshots/tab_settings.png')

render_switches()
render_settings()
print('Switches and Settings screenshots rendered successfully!')
