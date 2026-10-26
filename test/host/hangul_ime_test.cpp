// Host test for the 2-set composer: c++ -std=c++17 -I src test/host/hangul_ime_test.cpp src/hangul_ime.cpp
#include <cstdio>
#include <string>

#include "hangul_ime.h"

static void dropLastChar(std::string& s) {
  if (s.empty()) return;
  size_t i = s.size() - 1;
  while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) i--;
  s.erase(i);
}

static void applyEdit(std::string& s, const HangulEdit& e) {
  if (e.hadPreedit) dropLastChar(s);
  s += e.commit;
  s += e.preedit;
}

// Letters type jamo (uppercase = Shift), '<' is Backspace, ' ' commits and adds a space.
static std::string type(const char* keys) {
  hangulCommit();
  std::string s;
  for (const char* k = keys; *k; k++) {
    if (*k == '<') {
      HangulEdit e;
      if (hangulBackspace(&e)) applyEdit(s, e); else dropLastChar(s);
    } else if (*k == ' ') {
      hangulCommit();
      s += ' ';
    } else {
      const bool shift = *k >= 'A' && *k <= 'Z';
      const char lower = shift ? (char)(*k + 32) : *k;
      applyEdit(s, hangulInput(hangulJamoForKey((uint8_t)(0x04 + lower - 'a'), shift)));
    }
  }
  return s;
}

static int failures = 0;
static void check(const char* keys, const char* want) {
  const std::string got = type(keys);
  if (got != want) {
    std::printf("FAIL %-16s got [%s] want [%s]\n", keys, got.c_str(), want);
    failures++;
  }
}

int main() {
  check("dkssudgktpdy", "안녕하세요");
  check("gksrmf", "한글");
  check("rkqt", "값");
  check("rkqtk", "갑사");      // cluster splits when a vowel follows
  check("rkqk", "가바");       // final moves to the next syllable
  check("dhk", "와");
  check("dnjs", "원");
  check("dmldml", "의의");
  check("Tkd", "쌍");
  check("Ekfrl", "딸기");
  check("dkQk", "아빠");       // ㅃ cannot be a final
  check("rk Rk", "가 까");
  check("k", "ㅏ");
  check("hk", "ㅘ");           // lone compound vowel
  check("rr", "ㄱㄱ");
  check("ehfr", "돍");
  check("ekfr", "닭");
  check("ekfrk", "달가");
  check("dhkd", "왕");
  check("ekfr<", "달");       // backspace removes one jamo
  check("ekfr<<", "다");
  check("ekfr<<<", "ㄷ");
  check("ekfr<<<<", "");
  check("dhk<", "오");
  check("rkqtk<", "갑ㅅ");
  check("gks<<<<", "");
  check("gksrmf<<<", "한");    // preedit 글 is undone jamo by jamo
  check("gksrmf<<<<", "");     // then Backspace deletes whole syllables
  check("dO", "얘");
  check("vP", "폐");
  check("qnfrhrl", "불고기");
  check("dlfrdj", "읽어");
  check("Whr", "쪽");
  check("rhkswl", "관지");
  if (failures == 0) std::printf("all passed\n");
  return failures == 0 ? 0 : 1;
}
