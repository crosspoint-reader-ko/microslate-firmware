#include "EpdFontFamily.h"

#include <Utf8.h>

#include <algorithm>

const EpdFont* EpdFontFamily::getFont(const Style style) const {
  // Extract font style bits (ignore UNDERLINE bit for font selection)
  const bool hasBold = (style & BOLD) != 0;
  const bool hasItalic = (style & ITALIC) != 0;

  if (hasBold && hasItalic) {
    if (boldItalic) return boldItalic;
    if (bold) return bold;
    if (italic) return italic;
  } else if (hasBold && bold) {
    return bold;
  } else if (hasItalic && italic) {
    return italic;
  }

  return regular;
}

void EpdFontFamily::getTextDimensions(const char* string, int* w, int* h, const Style style) const {
  if (!fallback) {
    getFont(style)->getTextDimensions(string, w, h);
    return;
  }

  // Same bounds walk as EpdFont::getTextBounds, but glyphs may come from the fallback.
  int minX = 0, minY = 0, maxX = 0, maxY = 0;
  int cursorX = 0;
  uint32_t cp;
  while ((cp = utf8NextCodepoint(reinterpret_cast<const uint8_t**>(&string)))) {
    const EpdGlyph* glyph = getGlyph(cp, style);
    if (!glyph) glyph = getGlyph(REPLACEMENT_GLYPH, style);
    if (!glyph) continue;
    minX = std::min(minX, cursorX + glyph->left);
    maxX = std::max(maxX, cursorX + glyph->left + glyph->width);
    minY = std::min(minY, glyph->top - glyph->height);
    maxY = std::max(maxY, static_cast<int>(glyph->top));
    cursorX += glyph->advanceX;
  }
  *w = maxX - minX;
  *h = maxY - minY;
}

bool EpdFontFamily::hasPrintableChars(const char* string, const Style style) const {
  int w = 0, h = 0;
  getTextDimensions(string, &w, &h, style);
  return w > 0 || h > 0;
}

const EpdFontData* EpdFontFamily::getData(const Style style) const { return getFont(style)->data; }

const EpdGlyph* EpdFontFamily::getGlyph(const uint32_t cp, const Style style, const EpdFontData** data) const {
  const EpdFont* font = getFont(style);
  const EpdGlyph* glyph = font->getGlyph(cp);
  if (!glyph && fallback) {
    glyph = fallback->getGlyph(cp);
    if (glyph) font = fallback;
  }
  if (data) *data = font->data;
  return glyph;
}
