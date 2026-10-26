#!/bin/bash
# Editor fonts: RIDIBatang (Latin + all 11,172 Hangul syllables), NotoSans for
# whatever RIDIBatang lacks (Cyrillic, symbols). 1-bit to fit the app slot;
# threshold 4 keeps the thin serif strokes the way the 2-bit fonts show in BW.
set -e

cd "$(dirname "$0")"

KOREAN_FONT_SIZES=(12 14 16)

for size in ${KOREAN_FONT_SIZES[@]}; do
  font_name="ridibatang_${size}_regular"
  output_path="../builtinFonts/${font_name}.h"
  python fontconvert.py $font_name $size \
    ../builtinFonts/source/RIDIBatang/RIDIBatang.otf \
    ../builtinFonts/source/NotoSans/NotoSans-Regular.ttf \
    --threshold 4 \
    --additional-intervals 0x3000,0x3003 \
    --additional-intervals 0x3008,0x3011 \
    --additional-intervals 0x3131,0x318E \
    --additional-intervals 0xAC00,0xD7A3 \
    2>/dev/null > $output_path
  echo "Generated $output_path"
done
