import os
import sys

import cv2
import numpy as np
from PIL import Image
from scipy.ndimage import gaussian_filter, gaussian_filter1d

rng = np.random.default_rng(seed=1337)


def random_dither(gray):
    dither = np.array(gray)
    h, w = dither.shape
    for y in range(0, h - 1):
        for x in range(1, w - 1):
            old_pixel = dither[y, x]

            dither[y, x] = (
                1
                if ((1 - old_pixel) * (rng.random() / 2)) < 0.1 and old_pixel > 0
                else 0
            )
    return dither


def voronoi_labels(shape, p_gen):
    points = (255 * (rng.random(shape, dtype=float) > p_gen)).astype(np.uint8)
    _, labels = cv2.distanceTransformWithLabels(
        points, cv2.DIST_L2, 5, labelType=cv2.DIST_LABEL_PIXEL
    )
    return labels


def voronoi_dither(gray, p_gen=0.5):
    gray = np.asarray(gray, dtype=float)
    labels = voronoi_labels(gray.shape, p_gen)
    n = np.max(labels) + 1
    flat = labels.ravel()
    shade = np.bincount(flat, weights=gray.ravel(), minlength=n) / np.maximum(
        np.bincount(flat, minlength=n), 1
    )
    codes = rng.random(n, dtype=float) < shade
    return codes[labels].astype(float)


def floyd_steinberg_dither(gray):
    dither = np.array(gray) * 255
    h, w = dither.shape

    for y in range(h - 1):
        for x in range(1, w - 1):
            old_pixel = dither[y, x]

            new_pixel = 255 if old_pixel > 1 else 0
            dither[y, x] = new_pixel

            error = old_pixel - new_pixel

            dither[y, x + 1] += error * (7 / 16)
            dither[y + 1, x - 1] += error * (3 / 16)
            dither[y + 1, x] += error * (5 / 16)
            dither[y + 1, x + 1] += error * (1 / 16)

    output = np.clip(dither, 0, 255) / 255.0
    return output


SRC_DIR = "metno-weathericons/weather/png"

# manual freezing variants
FLAKED = {
    "freezing_rain": "rain",
    "freezing_drizzle": "lightrain",
    "icy_fog": "fog",
}


def load_src(img_name):
    if img_name not in FLAKED:
        return Image.open(f"{SRC_DIR}/{img_name}.png").convert("RGBA")

    img = Image.open(f"{SRC_DIR}/{FLAKED[img_name]}.png").convert("RGBA")
    flake = Image.open(f"{SRC_DIR}/snow.png").convert("RGBA").crop((60, 157, 87, 182))
    flake = flake.resize((54, 50), Image.LANCZOS)
    img.alpha_composite(flake, (15, 22))
    return img


def inkimg(img_name):
    img = load_src(img_name)
    np_gray = np.array(img.convert("L"))
    alpha = np.array(img.convert("RGBA"))[..., 3]

    content_mask = (255 * ((alpha > 127) & (np_gray < 250))).astype(np.uint8)
    n_regions, regions = cv2.connectedComponents(content_mask, connectivity=8)

    light_mask = np.zeros(np_gray.shape, dtype=bool)
    for r in range(1, n_regions):
        region = regions == r
        t = cv2.threshold(np_gray[region], 0, 255, cv2.THRESH_OTSU | cv2.THRESH_BINARY)[
            0
        ]
        light_mask |= region & (np_gray > t)

    output_np = np.ones((img.width, img.height))

    black_accent_mask = 1 - ((content_mask > 0) & ~light_mask)
    output_np *= black_accent_mask

    outside_u8 = (black_accent_mask > 0).astype(np.uint8) * 255
    d = cv2.distanceTransform(outside_u8, cv2.DIST_L2, cv2.DIST_MASK_PRECISE)

    halo_size = 15.0
    halo_mask = np.clip(d / halo_size, 0.0, 1.0)
    halo_mask[(content_mask == 0) | (black_accent_mask == 0)] = 1.0
    halo_mask **= 0.5

    dither = voronoi_dither(1 - halo_mask)
    output_np = np.clip((255 * output_np) - dither * (255 * (content_mask > 0)), 0, 255)

    contours, _ = cv2.findContours(
        content_mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE
    )
    outline = np.full(output_np.shape, 255, np.uint8)
    for c in contours:
        pts = gaussian_filter1d(c[:, 0, :].astype(float), 1.0, axis=0, mode="wrap")
        k = len(pts)

        if k >= 12:
            jitter = gaussian_filter(
                rng.normal(size=(k, 2)), (max(k / 30, 2), 0), mode="wrap"
            )
            pts += jitter / (np.abs(jitter).max() + 1e-9) * 1.0

        cv2.polylines(
            outline,
            [np.round(pts * 16).astype(np.int32)],
            True,
            0,
            3,
            cv2.LINE_AA,
            shift=4,
        )
    outline_mask = outline < 128
    output_np[outline_mask] = 0

    # (mode, p_gen, p_active, thickness, edges_only)
    noise_cfg = [
        ("cells", 0.5, 0.05, 1, False),
        # ("cracks", 0.002, 0.5, 1, True),
    ]
    for mode, p_gen, p_active, thickness, edges_only in noise_cfg:
        labels = voronoi_labels(output_np.shape, p_gen)
        codes = rng.random(np.max(labels) + 1, dtype=float) <= p_active

        if mode == "cells":
            noise_i = codes[labels]
        elif mode == "cracks":
            border = np.zeros(labels.shape, dtype=bool)
            border[:, 1:] |= labels[:, 1:] != labels[:, :-1]
            border[1:, :] |= labels[1:, :] != labels[:-1, :]
            noise_i = border & codes[labels]
            if thickness > 1:
                kernel = np.ones((thickness, thickness), np.uint8)
                noise_i = cv2.dilate(noise_i.astype(np.uint8), kernel) > 0
        else:
            continue

        if edges_only:
            noise_i &= outline_mask
        output_np[noise_i] = 255

    output_np = np.clip(output_np, 0, 255).astype(np.uint8)
    Image.fromarray(output_np).save(f"../../assets/weather/{img_name}.png")

    print(
        f"\x1b[38;5;139m\x1b[1mINFO:\x1b[0m wrote to kindle/assets/weather/{img_name}.png"
    )


DEFAULT_FILES = [
    "freezing_rain",
    "lightrain",
    "cloudy",
    "fair_night",
    "icy_fog",
    "rainandthunder",
    "clearsky_night",
    "heavyrain",
    "rain",
    "fog",
    "sleetandthunder",
    "heavysnow",
    "lightsnow",
    "freezing_drizzle",
    "snow",
    "heavysleetandthunder",
]


def main():
    if len(sys.argv) not in (2, 3):
        print(
            f"\x1b[38;5;139m\x1b[1mHELP:\x1b[0m {sys.argv[0]} [ -img image_name ] -all"
        )
        sys.exit(1)

    if sys.argv[1] == "-img":
        inkimg(sys.argv[2])
    elif sys.argv[1] == "-all":
        succ = 0

        input(
            "\x1b[38;5;139m\x1b[1mWARN:\x1b[0m this will clear \x1b[38;5;139massets/weather\x1b[0m. enter to continue\n"
        )
        os.system("rm ../../assets/weather/* 2>/dev/null")

        print(
            f"\x1b[38;5;139m\x1b[1mINFO:\x1b[0m preparing to convert \x1b[38;5;139m{len(DEFAULT_FILES)}\x1b[0m files"
        )
        for file in DEFAULT_FILES:
            try:
                inkimg(file)
                succ += 1
            except Exception as e:
                print(
                    f"\x1b[38;5;139m\x1b[1mERR: \x1b[0m inking({file}) failed - {e!s}"
                )

        if succ == len(DEFAULT_FILES):
            print("\x1b[38;5;139m\x1b[1mINFO: all done <3\x1b[0m")
        else:
            print(
                f"\x1b[38;5;139m\x1b[1mINFO: only converted {succ}/{len(DEFAULT_FILES)} requested files. check logs for more info.\x1b[0m"
            )

    else:
        print(
            f"\x1b[38;5;139m\x1b[1mERR:\x1b[0m  unknown mode \x1b[38;5;139m{sys.argv[1]}\x1b[0m"
        )


if __name__ == "__main__":
    main()
