import os
import math
from PIL import Image, ImageDraw, ImageFilter

def create_master_prism_icon(size=1024, is_round=False):
    # Supersampled canvas
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    center_x = size / 2.0
    center_y = size / 2.0

    # Background shape: squircle or circle
    bg_img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    bg_draw = ImageDraw.Draw(bg_img)
    margin = int(size * 0.04)

    if is_round:
        bg_draw.ellipse((margin, margin, size - margin, size - margin), fill=(9, 13, 22, 255))
    else:
        # Material 3 squircle rounded rect
        radius = int(size * 0.22)
        bg_draw.rounded_rectangle((margin, margin, size - margin, size - margin), radius=radius, fill=(9, 13, 22, 255))

    # Add outer subtle border
    if is_round:
        bg_draw.ellipse((margin, margin, size - margin, size - margin), outline=(0, 229, 255, 60), width=int(size * 0.015))
    else:
        radius = int(size * 0.22)
        bg_draw.rounded_rectangle((margin, margin, size - margin, size - margin), radius=radius, outline=(0, 229, 255, 60), width=int(size * 0.015))

    img = Image.alpha_composite(img, bg_img)

    # Ambient radial glow behind prism
    glow_img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    glow_draw = ImageDraw.Draw(glow_img)
    glow_radius = int(size * 0.38)
    for r in range(glow_radius, int(size * 0.08), -int(size * 0.03)):
        alpha = int(45 * (1.0 - (r / glow_radius)))
        glow_draw.ellipse(
            (center_x - r, center_y - r * 0.8, center_x + r, center_y + r * 0.8),
            fill=(2, 132, 199, alpha)
        )
    glow_img = glow_img.filter(ImageFilter.GaussianBlur(radius=int(size * 0.04)))
    img = Image.alpha_composite(img, glow_img)

    # Incident white laser beam entering from left
    beam_img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    beam_draw = ImageDraw.Draw(beam_img)
    p_in_start = (int(size * 0.08), int(size * 0.58))
    p_in_hit = (int(size * 0.36), int(size * 0.52))
    beam_draw.line([p_in_start, p_in_hit], fill=(255, 255, 255, 240), width=int(size * 0.022))
    # Beam glow
    beam_draw.line([p_in_start, p_in_hit], fill=(0, 229, 255, 120), width=int(size * 0.045))
    img = Image.alpha_composite(img, beam_img)

    # Refracted dispersion rays emerging from right facet
    refract_img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    refract_draw = ImageDraw.Draw(refract_img)
    p_exit_top = (int(size * 0.61), int(size * 0.44))
    p_exit_mid = (int(size * 0.63), int(size * 0.53))
    p_exit_bot = (int(size * 0.65), int(size * 0.62))

    p_dest_cyan = (int(size * 0.94), int(size * 0.33))
    p_dest_green = (int(size * 0.95), int(size * 0.44))
    p_dest_amber = (int(size * 0.95), int(size * 0.56))
    p_dest_rose = (int(size * 0.93), int(size * 0.68))

    # Cyan-Blue ray
    refract_draw.line([p_exit_top, p_dest_cyan], fill=(0, 229, 255, 240), width=int(size * 0.022))
    refract_draw.line([p_exit_top, p_dest_cyan], fill=(0, 229, 255, 90), width=int(size * 0.045))

    # Mint Green ray
    refract_draw.line([p_exit_top, p_dest_green], fill=(16, 185, 129, 240), width=int(size * 0.022))
    refract_draw.line([p_exit_top, p_dest_green], fill=(16, 185, 129, 90), width=int(size * 0.045))

    # Amber ray
    refract_draw.line([p_exit_mid, p_dest_amber], fill=(245, 158, 11, 240), width=int(size * 0.022))
    refract_draw.line([p_exit_mid, p_dest_amber], fill=(245, 158, 11, 90), width=int(size * 0.045))

    # Rose ray
    refract_draw.line([p_exit_bot, p_dest_rose], fill=(244, 63, 94, 240), width=int(size * 0.022))
    refract_draw.line([p_exit_bot, p_dest_rose], fill=(244, 63, 94, 90), width=int(size * 0.045))

    img = Image.alpha_composite(img, refract_img)

    # Glass Crystalline Prism
    prism_img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    p_draw = ImageDraw.Draw(prism_img)

    v_top = (int(size * 0.50), int(size * 0.22))
    v_left = (int(size * 0.25), int(size * 0.72))
    v_right = (int(size * 0.75), int(size * 0.72))

    # Main body: deep obsidian translucent glass with cyan gradient tint
    p_draw.polygon([v_top, v_left, v_right], fill=(15, 23, 42, 230))

    # Facet 1 (Left): Illuminated
    p_draw.line([v_top, v_left], fill=(0, 229, 255, 255), width=int(size * 0.026))
    p_draw.line([v_top, v_left], fill=(255, 255, 255, 160), width=int(size * 0.012))

    # Facet 2 (Right): Dispersion boundary
    p_draw.line([v_top, v_right], fill=(99, 102, 241, 240), width=int(size * 0.026))
    p_draw.line([v_top, v_right], fill=(168, 85, 247, 180), width=int(size * 0.014))

    # Facet 3 (Base):
    p_draw.line([v_left, v_right], fill=(30, 41, 59, 240), width=int(size * 0.020))

    # Inner reflective core (Subtle geometric triangle)
    v_inner_top = (int(size * 0.50), int(size * 0.38))
    v_inner_left = (int(size * 0.36), int(size * 0.65))
    v_inner_right = (int(size * 0.64), int(size * 0.65))
    p_draw.polygon([v_inner_top, v_inner_left, v_inner_right], fill=(0, 229, 255, 30), outline=(0, 229, 255, 80), width=int(size * 0.006))

    # Top apex glint / starburst
    glint_x, glint_y = v_top
    glint_r = int(size * 0.035)
    p_draw.ellipse((glint_x - glint_r, glint_y - glint_r, glint_x + glint_r, glint_y + glint_r), fill=(255, 255, 255, 255))
    p_draw.ellipse((glint_x - glint_r * 2, glint_y - glint_r * 2, glint_x + glint_r * 2, glint_y + glint_r * 2), fill=(0, 229, 255, 100))

    img = Image.alpha_composite(img, prism_img)
    return img

def main():
    densities = {
        "mipmap-mdpi": 48,
        "mipmap-hdpi": 72,
        "mipmap-xhdpi": 96,
        "mipmap-xxhdpi": 144,
        "mipmap-xxxhdpi": 192,
    }

    base_dirs = [
        os.path.abspath("android_app/apk_root/res"),
        os.path.abspath("android/src/main/res"),
    ]

    print("Generating Master 1024x1024 Prism-V Icons...")
    master_square = create_master_prism_icon(size=1024, is_round=False)
    master_round = create_master_prism_icon(size=1024, is_round=True)

    for base_res in base_dirs:
        for folder, target_size in densities.items():
            dir_path = os.path.join(base_res, folder)
            os.makedirs(dir_path, exist_ok=True)

            sq_resized = master_square.resize((target_size, target_size), Image.Resampling.LANCZOS)
            rd_resized = master_round.resize((target_size, target_size), Image.Resampling.LANCZOS)

            sq_path = os.path.join(dir_path, "ic_launcher.png")
            rd_path = os.path.join(dir_path, "ic_launcher_round.png")

            sq_resized.save(sq_path, "PNG")
            rd_resized.save(rd_path, "PNG")
            print(f"Saved {sq_path} ({target_size}x{target_size})")

        # Also save high-res notification small icon
        draw_dir = os.path.join(base_res, "drawable")
        os.makedirs(draw_dir, exist_ok=True)
        stat_icon = master_round.resize((96, 96), Image.Resampling.LANCZOS)
        stat_icon.save(os.path.join(draw_dir, "ic_stat_prism.png"), "PNG")

    print("All mipmap and drawable launcher icons successfully generated!")

if __name__ == "__main__":
    main()
