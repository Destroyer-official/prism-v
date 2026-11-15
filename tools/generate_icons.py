import os
import sys
from PIL import Image, ImageDraw, ImageFilter

def create_master_icons(source_path):
    raw_img = Image.open(source_path).convert("RGBA")
    w, h = raw_img.size
    if (w, h) != (1024, 1024):
        raw_img = raw_img.resize((1024, 1024), Image.Resampling.LANCZOS)

    # 1. Master Square / Squircle with smooth anti-aliased corner transparency
    mask_hi = Image.new("L", (4096, 4096), 0)
    d_m = ImageDraw.Draw(mask_hi)
    d_m.rounded_rectangle((0, 0, 4095, 4095), radius=880, fill=255)
    mask_sq = mask_hi.resize((1024, 1024), Image.Resampling.LANCZOS)
    master_square = raw_img.copy()
    master_square.putalpha(mask_sq)

    # 2. Master Round with subtle ambient cyan rim and anti-aliased circular alpha mask
    rim = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))
    d_rim = ImageDraw.Draw(rim)
    d_rim.ellipse((10, 10, 1014, 1014), outline=(0, 229, 255, 140), width=4)
    glow = rim.filter(ImageFilter.GaussianBlur(radius=2))
    combined_round = Image.alpha_composite(raw_img, glow)

    mask_round_hi = Image.new("L", (4096, 4096), 0)
    d_mr = ImageDraw.Draw(mask_round_hi)
    d_mr.ellipse((16, 16, 4080, 4080), fill=255)
    mask_round = mask_round_hi.resize((1024, 1024), Image.Resampling.LANCZOS)
    master_round = combined_round.copy()
    master_round.putalpha(mask_round)

    return raw_img, master_square, master_round

def main():
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    default_src = os.path.join(repo_root, "icon.png")

    if len(sys.argv) > 1 and os.path.exists(sys.argv[1]):
        source_path = sys.argv[1]
    elif os.path.exists(default_src):
        source_path = default_src
    else:
        print(f"Error: Icon source not found at {default_src}")
        sys.exit(1)

    print(f"Loading master icon from: {source_path}")
    raw_img, master_square, master_round = create_master_icons(source_path)

    # Save master icon.png in repo root
    master_out = os.path.join(repo_root, "icon.png")
    raw_img.save(master_out, "PNG")
    print(f"Updated master icon: {master_out}")

    densities = {
        "mipmap-mdpi": 48,
        "mipmap-hdpi": 72,
        "mipmap-xhdpi": 96,
        "mipmap-xxhdpi": 144,
        "mipmap-xxxhdpi": 192,
    }

    base_dirs = [
        os.path.join(repo_root, "android_app", "apk_root", "res"),
        os.path.join(repo_root, "android", "src", "main", "res"),
    ]

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

        # Save notification small icon
        draw_dir = os.path.join(base_res, "drawable")
        os.makedirs(draw_dir, exist_ok=True)
        stat_icon = master_round.resize((96, 96), Image.Resampling.LANCZOS)
        stat_path = os.path.join(draw_dir, "ic_stat_prism.png")
        stat_icon.save(stat_path, "PNG")
        print(f"Saved notification icon: {stat_path}")

    print("All mipmap and drawable launcher icons successfully generated!")

if __name__ == "__main__":
    main()
