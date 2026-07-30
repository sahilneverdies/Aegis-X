import struct

def create_aegis_ico(filepath):
    width = 32
    height = 32
    
    # 32-bit BGRA raw pixels (bottom-up for Windows BMP)
    pixels = bytearray()
    
    for y in range(height - 1, -1, -1):
        for x in range(width):
            # Center coordinates (-16 to +15)
            cx = x - 15.5
            cy = y - 15.5
            
            # Shield shape formula: abs(x) <= 12 and y >= -12 and y <= 12 - abs(x)*0.5
            abs_x = abs(cx)
            
            # Shield boundary check
            is_shield = False
            is_border = False
            
            if abs_x <= 12 and cy >= -13 and cy <= 11 - (abs_x * 0.4):
                is_shield = True
                if abs_x >= 10 or cy <= -11 or cy >= 9 - (abs_x * 0.4):
                    is_border = True
            
            # Aegis Core Star / Cross Emblem
            is_emblem = False
            if is_shield and not is_border:
                if (abs(cx) <= 2 and abs(cy) <= 7) or (abs(cy) <= 2 and abs(cx) <= 7):
                    is_emblem = True
            
            if is_emblem:
                # Electric Cyan (#00E5FF) -> BGRA: 255, 229, 0, 255
                b, g, r, a = 255, 229, 0, 255
            elif is_border:
                # Matrix Emerald Green (#00F5A0) -> BGRA: 160, 245, 0, 255
                b, g, r, a = 160, 245, 0, 255
            elif is_shield:
                # Cyber Obsidian Panel (#161B22) -> BGRA: 34, 27, 22, 255
                b, g, r, a = 34, 27, 22, 255
            else:
                # Transparent outer pixels
                b, g, r, a = 0, 0, 0, 0
                
            pixels.extend([b, g, r, a])

    # AND mask (1 bit per pixel, 32x32 bits = 128 bytes)
    and_mask = bytearray(128)

    # Header construction
    bmp_header = struct.pack('<IIIHHIIIIII',
        40,          # biSize
        width,       # biWidth
        height * 2,  # biHeight (64 for XOR + AND mask)
        1,           # biPlanes
        32,          # biBitCount
        0,           # biCompression (BI_RGB)
        len(pixels), # biSizeImage
        0, 0, 0, 0
    )

    image_data = bmp_header + pixels + and_mask
    image_size = len(image_data)

    ico_header = struct.pack('<HHH', 0, 1, 1) # idReserved=0, idType=1, idCount=1
    dir_entry = struct.pack('<BBBBHHII',
        width, height, 0, 0, 1, 32, image_size, 22
    )

    with open(filepath, 'wb') as f:
        f.write(ico_header + dir_entry + image_data)

    print(f"Successfully generated Aegis-X icon at {filepath}")

if __name__ == '__main__':
    create_aegis_ico('e:/cs2 anticheats/CS2AC_FOW/res/aegisx.ico')
