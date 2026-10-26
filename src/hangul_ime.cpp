#include "hangul_ime.h"

#include <cstring>

// All jamo are kept as Hangul Compatibility Jamo code points (U+3131..U+3163),
// the same characters the composer prints for a lone consonant or vowel.

namespace {

struct State {
  uint16_t cho;   // initial consonant, 0 = none
  uint16_t jung;  // vowel, 0 = none
  uint16_t jong;  // final consonant (may be a cluster such as ㄳ), 0 = none
};

constexpr uint32_t VOWEL_FIRST = 0x314F;  // ㅏ
constexpr uint32_t VOWEL_LAST = 0x3163;   // ㅣ

// Initial consonants in syllable-block order (index = choseong index).
constexpr uint16_t CHO[19] = {0x3131, 0x3132, 0x3134, 0x3137, 0x3138, 0x3139, 0x3141, 0x3142, 0x3143, 0x3145,
                              0x3146, 0x3147, 0x3148, 0x3149, 0x314A, 0x314B, 0x314C, 0x314D, 0x314E};

// Final consonants in syllable-block order (index 0 = no final).
constexpr uint16_t JONG[28] = {0,      0x3131, 0x3132, 0x3133, 0x3134, 0x3135, 0x3136, 0x3137, 0x3139, 0x313A,
                               0x313B, 0x313C, 0x313D, 0x313E, 0x313F, 0x3140, 0x3141, 0x3142, 0x3144, 0x3145,
                               0x3146, 0x3147, 0x3148, 0x314A, 0x314B, 0x314C, 0x314D, 0x314E};

struct Pair {
  uint16_t a, b, ab;
};

constexpr Pair VOWEL_PAIRS[] = {
    {0x3157, 0x314F, 0x3158},  // ㅗ+ㅏ=ㅘ
    {0x3157, 0x3150, 0x3159},  // ㅗ+ㅐ=ㅙ
    {0x3157, 0x3163, 0x315A},  // ㅗ+ㅣ=ㅚ
    {0x315C, 0x3153, 0x315D},  // ㅜ+ㅓ=ㅝ
    {0x315C, 0x3154, 0x315E},  // ㅜ+ㅔ=ㅞ
    {0x315C, 0x3163, 0x315F},  // ㅜ+ㅣ=ㅟ
    {0x3161, 0x3163, 0x3162},  // ㅡ+ㅣ=ㅢ
};

constexpr Pair JONG_PAIRS[] = {
    {0x3131, 0x3145, 0x3133},  // ㄱ+ㅅ=ㄳ
    {0x3134, 0x3148, 0x3135},  // ㄴ+ㅈ=ㄵ
    {0x3134, 0x314E, 0x3136},  // ㄴ+ㅎ=ㄶ
    {0x3139, 0x3131, 0x313A},  // ㄹ+ㄱ=ㄺ
    {0x3139, 0x3141, 0x313B},  // ㄹ+ㅁ=ㄻ
    {0x3139, 0x3142, 0x313C},  // ㄹ+ㅂ=ㄼ
    {0x3139, 0x3145, 0x313D},  // ㄹ+ㅅ=ㄽ
    {0x3139, 0x314C, 0x313E},  // ㄹ+ㅌ=ㄾ
    {0x3139, 0x314D, 0x313F},  // ㄹ+ㅍ=ㄿ
    {0x3139, 0x314E, 0x3140},  // ㄹ+ㅎ=ㅀ
    {0x3142, 0x3145, 0x3144},  // ㅂ+ㅅ=ㅄ
};

// 2-set layout for HID letter keys a..z: {plain, shifted}.
constexpr uint16_t KEYMAP[26][2] = {
    {0x3141, 0x3141},  // a ㅁ
    {0x3160, 0x3160},  // b ㅠ
    {0x314A, 0x314A},  // c ㅊ
    {0x3147, 0x3147},  // d ㅇ
    {0x3137, 0x3138},  // e ㄷ ㄸ
    {0x3139, 0x3139},  // f ㄹ
    {0x314E, 0x314E},  // g ㅎ
    {0x3157, 0x3157},  // h ㅗ
    {0x3151, 0x3151},  // i ㅑ
    {0x3153, 0x3153},  // j ㅓ
    {0x314F, 0x314F},  // k ㅏ
    {0x3163, 0x3163},  // l ㅣ
    {0x3161, 0x3161},  // m ㅡ
    {0x315C, 0x315C},  // n ㅜ
    {0x3150, 0x3152},  // o ㅐ ㅒ
    {0x3154, 0x3156},  // p ㅔ ㅖ
    {0x3142, 0x3143},  // q ㅂ ㅃ
    {0x3131, 0x3132},  // r ㄱ ㄲ
    {0x3134, 0x3134},  // s ㄴ
    {0x3145, 0x3146},  // t ㅅ ㅆ
    {0x3155, 0x3155},  // u ㅕ
    {0x314D, 0x314D},  // v ㅍ
    {0x3148, 0x3149},  // w ㅈ ㅉ
    {0x314C, 0x314C},  // x ㅌ
    {0x315B, 0x315B},  // y ㅛ
    {0x314B, 0x314B},  // z ㅋ
};

constexpr int HISTORY_MAX = 8;

State cur = {0, 0, 0};
// States before each jamo of the syllable in progress, for jamo-wise backspace.
State history[HISTORY_MAX];
int historyLen = 0;

bool isVowel(uint32_t c) { return c >= VOWEL_FIRST && c <= VOWEL_LAST; }

int indexOf(const uint16_t* table, int n, uint32_t c) {
  for (int i = 0; i < n; i++) {
    if (table[i] == c) return i;
  }
  return -1;
}

uint16_t combine(const Pair* pairs, int n, uint32_t a, uint32_t b) {
  for (int i = 0; i < n; i++) {
    if (pairs[i].a == a && pairs[i].b == b) return pairs[i].ab;
  }
  return 0;
}

bool splitCluster(uint32_t ab, uint16_t* a, uint16_t* b) {
  for (const Pair& p : JONG_PAIRS) {
    if (p.ab == ab) {
      *a = p.a;
      *b = p.b;
      return true;
    }
  }
  return false;
}

bool isEmpty(const State& s) { return s.cho == 0 && s.jung == 0 && s.jong == 0; }

uint32_t charOf(const State& s) {
  if (s.cho && s.jung) {
    const int c = indexOf(CHO, 19, s.cho);
    const int j = s.jong ? indexOf(JONG, 28, s.jong) : 0;
    if (c >= 0 && j >= 0) return 0xAC00 + (c * 21 + (s.jung - VOWEL_FIRST)) * 28 + j;
  }
  if (s.cho) return s.cho;
  if (s.jung) return s.jung;
  return 0;
}

void encode(uint32_t cp, char out[4]) {
  if (cp == 0) {
    out[0] = '\0';
  } else if (cp < 0x80) {
    out[0] = (char)cp;
    out[1] = '\0';
  } else if (cp < 0x800) {
    out[0] = (char)(0xC0 | (cp >> 6));
    out[1] = (char)(0x80 | (cp & 0x3F));
    out[2] = '\0';
  } else {
    // Every code point this composer emits is in the BMP below U+FFFF.
    out[0] = (char)(0xE0 | (cp >> 12));
    out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[2] = (char)(0x80 | (cp & 0x3F));
    out[3] = '\0';
  }
}

void push(const State& next) {
  if (historyLen == HISTORY_MAX) {
    memmove(history, history + 1, sizeof(State) * (HISTORY_MAX - 1));
    historyLen--;
  }
  history[historyLen++] = cur;
  cur = next;
}

// Finish `done` and start a fresh syllable holding `jamo`.
void restart(HangulEdit* e, const State& done, uint32_t jamo) {
  encode(charOf(done), e->commit);
  historyLen = 0;
  cur = {0, 0, 0};
  State next = {0, 0, 0};
  if (isVowel(jamo)) {
    next.jung = jamo;
  } else {
    next.cho = jamo;
  }
  push(next);
}

}  // namespace

uint32_t hangulJamoForKey(uint8_t hidKey, bool shift) {
  // HID usage 0x04 = 'a' ... 0x1D = 'z'
  if (hidKey < 0x04 || hidKey > 0x1D) return 0;
  return KEYMAP[hidKey - 0x04][shift ? 1 : 0];
}

bool hangulIsComposing() { return !isEmpty(cur); }

void hangulCommit() {
  cur = {0, 0, 0};
  historyLen = 0;
}

HangulEdit hangulInput(uint32_t jamo) {
  HangulEdit e;
  e.hadPreedit = hangulIsComposing();
  e.commit[0] = '\0';

  State next = cur;

  if (isVowel(jamo)) {
    if (cur.jung == 0) {
      // empty, or a lone initial consonant waiting for its vowel
      next.jung = jamo;
      push(next);
    } else if (cur.jong == 0) {
      const uint16_t v = combine(VOWEL_PAIRS, sizeof(VOWEL_PAIRS) / sizeof(Pair), cur.jung, jamo);
      if (v) {
        next.jung = v;
        push(next);
      } else {
        restart(&e, cur, jamo);
      }
    } else {
      // The final consonant moves to the next syllable: 갑+ㅏ = 가바, 값+ㅏ = 갑사.
      State done = cur;
      uint16_t keep = 0, move = cur.jong;
      if (splitCluster(cur.jong, &keep, &move)) {
        done.jong = keep;
      } else {
        done.jong = 0;
      }
      restart(&e, done, move);
      next = cur;
      next.jung = jamo;
      push(next);
    }
  } else if (jamo >= 0x3131 && jamo < VOWEL_FIRST) {
    if (cur.cho == 0 && cur.jung == 0) {
      next.cho = jamo;
      push(next);
    } else if (cur.jung == 0 || cur.cho == 0) {
      // a lone consonant or a lone vowel cannot take this consonant
      restart(&e, cur, jamo);
    } else if (cur.jong == 0) {
      if (indexOf(JONG, 28, jamo) > 0) {
        next.jong = jamo;
        push(next);
      } else {
        restart(&e, cur, jamo);  // ㄸ ㅃ ㅉ never end a syllable
      }
    } else {
      const uint16_t c = combine(JONG_PAIRS, sizeof(JONG_PAIRS) / sizeof(Pair), cur.jong, jamo);
      if (c) {
        next.jong = c;
        push(next);
      } else {
        restart(&e, cur, jamo);
      }
    }
  }

  encode(charOf(cur), e.preedit);
  return e;
}

bool hangulBackspace(HangulEdit* out) {
  if (!hangulIsComposing()) return false;
  out->hadPreedit = true;
  out->commit[0] = '\0';
  if (historyLen > 0) {
    cur = history[--historyLen];
  } else {
    cur = {0, 0, 0};
  }
  encode(charOf(cur), out->preedit);
  return true;
}
