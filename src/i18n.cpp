#include "i18n.h"

static Language g_language = Language::ENGLISH;

static const char* const kEnglish[] = {
#define I18N_EN(id, en, ko) en,
  I18N_STRINGS(I18N_EN)
#undef I18N_EN
};

static const char* const kKorean[] = {
#define I18N_KO(id, en, ko) ko,
  I18N_STRINGS(I18N_KO)
#undef I18N_KO
};

static_assert(sizeof(kEnglish) / sizeof(kEnglish[0]) == static_cast<std::size_t>(StrId::COUNT), "English table size");
static_assert(sizeof(kKorean) / sizeof(kKorean[0]) == static_cast<std::size_t>(StrId::COUNT), "Korean table size");

Language getLanguage() { return g_language; }
void setLanguage(Language lang) { g_language = lang; }

Language languageFromIndex(int index) {
  return (index == static_cast<int>(Language::KOREAN)) ? Language::KOREAN : Language::ENGLISH;
}

Language languageNext(Language lang) {
  return languageFromIndex((static_cast<int>(lang) + 1) % LANGUAGE_COUNT);
}

Language languagePrev(Language lang) {
  return languageFromIndex((static_cast<int>(lang) + LANGUAGE_COUNT - 1) % LANGUAGE_COUNT);
}

const char* languageName(Language lang) {
  return lang == Language::KOREAN ? "한국어" : "English";
}

const char* tr(StrId id) {
  const std::size_t i = static_cast<std::size_t>(id);
  if (i >= static_cast<std::size_t>(StrId::COUNT)) return "";
  return (g_language == Language::KOREAN) ? kKorean[i] : kEnglish[i];
}
