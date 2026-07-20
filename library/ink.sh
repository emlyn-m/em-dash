#!/usr/bin/env bash

set -euo pipefail

EDGE="canny"
LOW_THRESHOLD=0.1
HIGH_THRESHOLD=0.4
EDGE_THRESHOLD=0.10
CONTRAST=1.6
CORRECTION=1.6
STIPPLE_SIZE=1.0
STIPPLE=1
TRANSPARENT=1
PAPER="Assets/ivory-off-white-paper-texture.jpg"
OUT="ink_out.png"
SEED=""

usage() {
    sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'
    cat <<EOF

Usage: ink.sh [options] <input-image> [output-image]

  -e, --edge METHOD        contrast | sobel | prewitt | canny   (default: $EDGE)
      --low N              canny low threshold                  (default: $LOW_THRESHOLD)
      --high N             canny high threshold                 (default: $HIGH_THRESHOLD)
      --edge-threshold N   binarize sobel/prewitt/contrast      (default: $EDGE_THRESHOLD)
  -c, --contrast N         stipple contrast (_Contrast)         (default: $CONTRAST)
  -g, --correction N       stipple gamma (_LuminanceCorrection) (default: $CORRECTION)
  -s, --stipple-size N     dot fineness (>1 finer, <1 coarser)  (default: $STIPPLE_SIZE)
      --no-stipple         linework only, no stipple dots
      --flatten            fill the background with paper instead of transparency
  -p, --paper FILE         paper texture                        (default: $PAPER)
      --seed N             fix the blue-noise seed for repeatable dots
  -o, --output FILE        output path (or 2nd positional arg)  (default: $OUT)
  -h, --help

Examples:
  ./ink.sh photo.png
  ./ink.sh -e sobel --no-stipple drawing.png lines.png
  ./ink.sh -e canny --high 0.5 -c 2.0 -s 1.5 portrait.png out.png
EOF
}

# ----- arg parsing -----------------------------------------------------------
INPUT=""
POSITIONALS=()
while [[ $# -gt 0 ]]; do
    case "$1" in
        -e|--edge)           EDGE="$2"; shift 2;;
        --low)               LOW_THRESHOLD="$2"; shift 2;;
        --high)              HIGH_THRESHOLD="$2"; shift 2;;
        --edge-threshold)    EDGE_THRESHOLD="$2"; shift 2;;
        -c|--contrast)       CONTRAST="$2"; shift 2;;
        -g|--correction)     CORRECTION="$2"; shift 2;;
        -s|--stipple-size)   STIPPLE_SIZE="$2"; shift 2;;
        --no-stipple)        STIPPLE=0; shift;;
        --flatten)           TRANSPARENT=0; shift;;
        -p|--paper)          PAPER="$2"; shift 2;;
        --seed)              SEED="$2"; shift 2;;
        -o|--output)         OUT="$2"; shift 2;;
        -h|--help)           usage; exit 0;;
        -*)                  echo "Unknown option: $1" >&2; usage; exit 1;;
        *)                   POSITIONALS+=("$1"); shift;;
    esac
done

[[ ${#POSITIONALS[@]} -ge 1 ]] || { echo "error: no input image given" >&2; usage; exit 1; }
INPUT="${POSITIONALS[0]}"
[[ ${#POSITIONALS[@]} -ge 2 ]] && OUT="${POSITIONALS[1]}"
[[ -f "$INPUT" ]] || { echo "error: input not found: $INPUT" >&2; exit 1; }
[[ -f "$PAPER" ]] || { echo "error: paper texture not found: $PAPER" >&2; exit 1; }

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# ----- 0. prep ---------------------------------------------------------------
# Remember the original transparency (opaque white if the input had no alpha),
# then flatten onto white so the subject silhouette becomes an edge and the
# background luminance is well-defined for the passes below.
magick "$INPUT" -alpha extract "$TMP/alpha.png"
magick "$INPUT" -background white -alpha remove -alpha off "$TMP/src.png"
W=$(magick identify -format "%w" "$TMP/src.png")
H=$(magick identify -format "%h" "$TMP/src.png")

# ----- 1. Luminance (Rec.709, matches LinearRgbToLuminance) -------------------
magick "$TMP/src.png" -grayscale Rec709Luminance "$TMP/lum.png"

# ----- 2. Edge detection -----------------------------------------------------
case "$EDGE" in
    contrast)
        # local max - local min over the 5-px plus neighbourhood (N,E,S,W,center)
        magick "$TMP/lum.png" -morphology Dilate Diamond:1 "$TMP/dil.png"
        magick "$TMP/lum.png" -morphology Erode  Diamond:1 "$TMP/ero.png"
        magick "$TMP/dil.png" "$TMP/ero.png" -compose Minus_Src -composite \
               -threshold "$(awk "BEGIN{print $EDGE_THRESHOLD*100}")%" "$TMP/edge.png"
        ;;
    sobel|prewitt)
        K=$(echo "$EDGE" | sed 's/^./\U&/')   # Sobel / Prewitt
        magick "$TMP/lum.png" -define convolve:scale='!' -morphology Convolve "${K}:0"  "$TMP/gx.png"
        magick "$TMP/lum.png" -define convolve:scale='!' -morphology Convolve "${K}:90" "$TMP/gy.png"
        magick "$TMP/gx.png" "$TMP/gy.png" -fx 'hypot(u,v)' -clamp \
               -threshold "$(awk "BEGIN{print $EDGE_THRESHOLD*100}")%" "$TMP/edge.png"
        ;;
    canny)
        LO=$(awk "BEGIN{print $LOW_THRESHOLD*100}")
        HI=$(awk "BEGIN{print $HIGH_THRESHOLD*100}")
        magick "$TMP/lum.png" -canny "0x1+${LO}%+${HI}%" "$TMP/canny.png"
        # Line Width pass: thicken edges by 1px, but only into dark areas (L<=0.7)
        magick "$TMP/lum.png" -threshold 70% -negate "$TMP/dark.png"
        magick "$TMP/canny.png" -morphology Dilate Diamond:1 "$TMP/grown.png"
        magick "$TMP/grown.png" "$TMP/dark.png" -compose Multiply -composite "$TMP/growndark.png"
        magick "$TMP/canny.png" "$TMP/growndark.png" -compose Lighten -composite "$TMP/edge.png"
        ;;
    *)
        echo "error: unknown edge method '$EDGE'" >&2; exit 1;;
esac

# ----- 3. Stippling (blue-noise dither) --------------------------------------
if [[ "$STIPPLE" -eq 1 ]]; then
    # adjLum = pow( clamp( _Contrast*(L-0.5)+0.5 ), 1/_LuminanceCorrection )
    magick "$TMP/lum.png" -fx "clamp(${CONTRAST}*(u-0.5)+0.5)" -gamma "$CORRECTION" "$TMP/adj.png"

    # blue-noise-ish threshold map: white noise minus its blur = high-freq noise.
    # stipple-size scales the generation resolution: >1 -> finer dots, <1 -> coarser.
    BW=$(awk "BEGIN{v=int($W*$STIPPLE_SIZE+0.5); print (v<1?1:v)}")
    BH=$(awk "BEGIN{v=int($H*$STIPPLE_SIZE+0.5); print (v<1?1:v)}")
    SEEDARG=""; [[ -n "$SEED" ]] && SEEDARG="-seed $SEED"
    magick -size "${BW}x${BH}" $SEEDARG xc: +noise Random -grayscale Rec709Luminance \
           \( +clone -blur 0x1 \) -compose Minus_Src -composite -auto-level \
           -scale "${W}x${H}!" "$TMP/noise.png"

    # ink where adjusted luminance is darker than the noise threshold
    magick "$TMP/adj.png" "$TMP/noise.png" -fx 'u<v?1.0:0.0' "$TMP/stipple.png"
else
    magick -size "${W}x${H}" xc:black "$TMP/stipple.png"
fi

# ----- 4. Combine: ink = edges ∪ stipple  (white = ink) ----------------------
magick "$TMP/edge.png" "$TMP/stipple.png" -compose Lighten -composite "$TMP/ink.png"

# ----- 5. Color: paper where blank, inverted-paper ink where inked -----------
magick "$TMP/ink.png" -negate "$TMP/papermask.png"          # white where paper shows
magick "$PAPER" -resize "${W}x${H}^" -gravity center -extent "${W}x${H}" "$TMP/paper.png"
magick "$TMP/paper.png" -negate "$TMP/inkcol.png"           # ink = inverted paper
magick "$TMP/inkcol.png" "$TMP/paper.png" "$TMP/papermask.png" \
       -compose Over -composite "$TMP/rgb.png"

# Restore the original transparency (unless --flatten): the background stays clear
# and only the subject keeps its inked paper.
if [[ "$TRANSPARENT" -eq 1 ]]; then
    case "${OUT,,}" in
        *.png|*.webp|*.tif|*.tiff|*.gif) ;;
        *) echo "warning: --transparent output but '$OUT' is not an alpha-capable format" >&2;;
    esac
    magick "$TMP/rgb.png" "$TMP/alpha.png" -alpha off -compose CopyOpacity -composite "$OUT"
else
    cp "$TMP/rgb.png" "$OUT"
fi

echo "wrote $OUT  (${W}x${H}, edge=$EDGE, stipple=$([[ $STIPPLE -eq 1 ]] && echo on || echo off), bg=$([[ $TRANSPARENT -eq 1 ]] && echo transparent || echo paper))"
