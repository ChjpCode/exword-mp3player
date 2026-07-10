from PIL import Image, ImageDraw, ImageFont
import os

WIDTH = 528
HEIGHT = 320

def rgb_to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def generate_image():
    # Create a new image with dark background
    img = Image.new('RGB', (WIDTH, HEIGHT), color=(16, 20, 24))
    draw = ImageDraw.Draw(img)
    
    # Draw pixel-art retro stripes
    # Slanted lines approx 60 degrees. Let's use polygons.
    stripe_width = 15
    gap = 4
    
    def draw_slant(x_start, color):
        pts = [
            (x_start, 0),
            (x_start + stripe_width, 0),
            (x_start + stripe_width - 150, HEIGHT),
            (x_start - 150, HEIGHT)
        ]
        draw.polygon(pts, fill=color)

    # Cyan, Pink, Yellow
    draw_slant(300, (60, 220, 200)) # Cyan
    draw_slant(300 + stripe_width + gap, (250, 100, 150)) # Pink
    draw_slant(300 + (stripe_width + gap)*2, (250, 220, 100)) # Yellow

    # Draw WALK-MAN logo (simple geometric blocks)
    # W A L K - M A N
    logo_x = 350
    logo_y = 30
    draw.text((logo_x, logo_y), "WALK-MAN", fill=(200, 200, 200), font=ImageFont.load_default(), align="left")
    # Actually, default font is too small. We will just draw a nice large rectangle as a placeholder for the tape,
    # and some geometric shapes for the "WALK-MAN" text.
    
    # Let's draw the word WALK-MAN using large rectangles
    draw.text((360, 30), "W A L K . M A N", fill=(220, 220, 220), font=ImageFont.load_default()) # A bigger font would be nice, but default is fine if we scale it.
    
    # Draw Cassette Outline
    cassette_x = 220
    cassette_y = 120
    cassette_w = 280
    cassette_h = 160
    
    # Cassette body
    draw.rectangle([cassette_x, cassette_y, cassette_x + cassette_w, cassette_y + cassette_h], outline=(150, 150, 150), width=4, fill=(40, 45, 50))
    # Cassette label
    draw.rectangle([cassette_x + 20, cassette_y + 20, cassette_x + cassette_w - 20, cassette_y + cassette_h - 40], outline=(120, 120, 120), width=2, fill=(180, 190, 190))
    
    # Cassette holes
    hole_radius = 20
    # Left hole
    lx = cassette_x + 60
    ly = cassette_y + 60
    draw.ellipse([lx - hole_radius, ly - hole_radius, lx + hole_radius, ly + hole_radius], fill=(20, 20, 20))
    # Right hole
    rx = cassette_x + cassette_w - 60
    ry = cassette_y + 60
    draw.ellipse([rx - hole_radius, ry - hole_radius, rx + hole_radius, ry + hole_radius], fill=(20, 20, 20))
    
    # "STEREO" text on tape
    draw.text((cassette_x + 120, cassette_y + 55), "STEREO", fill=(20, 20, 20))
    draw.text((cassette_x + 110, cassette_y + 75), "NORMAL BIAS", fill=(20, 20, 20))
    
    # Cassette bottom trapeze
    draw.polygon([
        (cassette_x + 30, cassette_y + cassette_h - 30),
        (cassette_x + cassette_w - 30, cassette_y + cassette_h - 30),
        (cassette_x + cassette_w - 10, cassette_y + cassette_h),
        (cassette_x + 10, cassette_y + cassette_h)
    ], outline=(150, 150, 150), fill=(80, 80, 80))

    return img

def export_c_header(img, filename):
    pixels = img.load()
    with open(filename, 'w') as f:
        f.write("#ifndef BG_IMAGE_H\n")
        f.write("#define BG_IMAGE_H\n\n")
        f.write("const unsigned short bg_image[528 * 320] = {\n")
        
        count = 0
        for y in range(HEIGHT):
            for x in range(WIDTH):
                r, g, b = pixels[x, y]
                rgb565 = rgb_to_rgb565(r, g, b)
                f.write(f"0x{rgb565:04X}, ")
                count += 1
                if count % 16 == 0:
                    f.write("\n")
        
        f.write("\n};\n\n")
        f.write("#endif // BG_IMAGE_H\n")

if __name__ == "__main__":
    print("Generating UI image...")
    img = generate_image()
    header_path = os.path.join(os.path.dirname(__file__), "bg_image.h")
    export_c_header(img, header_path)
    print(f"Generated header at {header_path}")
