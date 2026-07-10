from PIL import Image
import os
import sys

WIDTH = 528
HEIGHT = 320

def rgb_to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

def convert_image(input_path, output_path):
    if not os.path.exists(input_path):
        print(f"Error: {input_path} not found.")
        sys.exit(1)
        
    img = Image.open(input_path).convert('RGB')
    img = img.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)
    
    pixels = img.load()
    with open(output_path, 'w') as f:
        f.write("#ifndef BG_IMAGE_H\n")
        f.write("#define BG_IMAGE_H\n\n")
        f.write(f"// Generated from {os.path.basename(input_path)}\n")
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
    print(f"Successfully converted {input_path} to {output_path}")

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("input", help="Path to input image (e.g., background.png)")
    parser.add_argument("output", help="Path to output header file (e.g., bg_image.h)")
    args = parser.parse_args()
    
    convert_image(args.input, args.output)
