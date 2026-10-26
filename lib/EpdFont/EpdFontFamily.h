#pragma once
#include "EpdFont.h"

class EpdFontFamily {
 public:
  enum Style : uint8_t { REGULAR = 0, BOLD = 1, ITALIC = 2, BOLD_ITALIC = 3, UNDERLINE = 4 };

  // `fallback` supplies glyphs the styled font lacks (e.g. Hangul for a Latin UI
  // font). It has one style only, so bold Hangul draws as regular.
  explicit EpdFontFamily(const EpdFont* regular, const EpdFont* bold = nullptr, const EpdFont* italic = nullptr,
                         const EpdFont* boldItalic = nullptr, const EpdFont* fallback = nullptr)
      : regular(regular), bold(bold), italic(italic), boldItalic(boldItalic), fallback(fallback) {}
  ~EpdFontFamily() = default;
  void getTextDimensions(const char* string, int* w, int* h, Style style = REGULAR) const;
  bool hasPrintableChars(const char* string, Style style = REGULAR) const;
  const EpdFontData* getData(Style style = REGULAR) const;
  // Glyph for `cp`, from the styled font or else the fallback. `data`, when
  // given, receives the font data the glyph's bitmap offset points into.
  const EpdGlyph* getGlyph(uint32_t cp, Style style = REGULAR, const EpdFontData** data = nullptr) const;

 private:
  const EpdFont* regular;
  const EpdFont* bold;
  const EpdFont* italic;
  const EpdFont* boldItalic;
  const EpdFont* fallback;

  const EpdFont* getFont(Style style) const;
};
