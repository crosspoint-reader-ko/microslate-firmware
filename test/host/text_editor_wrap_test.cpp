// Host test for pixel wrapping and in-place replace:
// c++ -std=c++17 -I src test/host/text_editor_wrap_test.cpp src/text_editor.cpp
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "text_editor.h"

// Latin 10 px, Hangul 20 px, everything else 10 px.
static int advance(uint32_t cp) { return (cp >= 0xAC00 && cp <= 0xD7A3) ? 20 : 10; }

static std::vector<std::string> lines() {
  std::vector<std::string> out;
  const char* buf = editorGetBuffer();
  const int n = editorGetLineCount();
  for (int i = 0; i < n; i++) {
    const int a = editorGetLinePosition(i);
    const int b = (i + 1 < n) ? editorGetLinePosition(i + 1) : (int)editorGetLength();
    out.emplace_back(buf + a, buf + b);
  }
  return out;
}

static void load(const char* text) {
  editorClear();
  strcpy(editorGetBuffer(), text);
  editorLoadBuffer(strlen(text));
}

static int failures = 0;
static void expect(const char* name, const std::vector<std::string>& want) {
  const auto got = lines();
  if (got != want) {
    failures++;
    std::printf("FAIL %s\n  got:", name);
    for (auto& l : got) std::printf(" [%s]", l.c_str());
    std::printf("\n  want:");
    for (auto& l : want) std::printf(" [%s]", l.c_str());
    std::printf("\n");
  }
}

int main() {
  editorInit();
  editorSetWrapWidth(100, 1, advance);  // 10 Latin chars or 5 Hangul per line

  load("hello world again");
  expect("latin words", {"hello ", "world ", "again"});

  load("가나다 라마바사");
  expect("hangul words", {"가나다 ", "라마바사"});

  load("가나다라마바사아자");
  expect("long hangul word breaks between chars", {"가나다라마", "바사아자"});

  load("ab\n가나");
  expect("hard newline", {"ab\n", "가나"});

  load("abcdefghij     x");
  expect("space run wraps after one hanging space", {"abcdefghij ", "    x"});

  load("한글 English 섞어");
  expect("mixed", {"한글 ", "English ", "섞어"});

  // Replace before cursor: the composer's syllable swap.
  load("가");
  editorReplaceBeforeCursor(true, "각");
  if (std::string(editorGetBuffer()) != "각") { failures++; std::printf("FAIL replace\n"); }
  editorReplaceBeforeCursor(false, "나");
  if (std::string(editorGetBuffer()) != "각나") { failures++; std::printf("FAIL append\n"); }

  // Typing over a selection replaces it.
  load("abc가");
  editorSelectAll();
  editorReplaceBeforeCursor(false, "ㄱ");
  if (std::string(editorGetBuffer()) != "ㄱ") { failures++; std::printf("FAIL selection [%s]\n", editorGetBuffer()); }

  if (failures == 0) std::printf("all passed\n");
  return failures == 0 ? 0 : 1;
}
