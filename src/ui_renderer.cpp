#include "ui_renderer.h"
#include "config.h"
#include "text_editor.h"
#include "file_manager.h"
#include "ble_keyboard.h"
#include "wifi_sync.h"
#include "keyboard_layout.h"
#include "i18n.h"

#include <GfxRenderer.h>
#include <HalGPIO.h>
#include <HalDisplay.h>
#include <EpdFont.h>
#include <EpdFontFamily.h>

// External variables
extern bool autoReconnectEnabled;
extern bool darkMode;
extern bool cleanMode;
extern bool deleteConfirmPending;
extern WritingMode writingMode;
extern FontSize fontSize;
extern bool showWordCount;
extern KeyboardLayout currentKeyboardLayout;

// External functions
uint32_t getCurrentPasskey();
bool isDeviceScanning();
uint32_t getScanAgeMs();

// Font data includes
#include <builtinFonts/notosans_16_regular.h>
#include <builtinFonts/notosans_16_bold.h>
#include <builtinFonts/notosans_14_regular.h>
#include <builtinFonts/notosans_14_bold.h>
#include <builtinFonts/notosans_12_regular.h>
#include <builtinFonts/notosans_12_bold.h>
#include <builtinFonts/ubuntu_10_regular.h>
#include <builtinFonts/ubuntu_10_bold.h>
#include <builtinFonts/ridibatang_12_regular.h>
#include <builtinFonts/ridibatang_14_regular.h>
#include <builtinFonts/ridibatang_16_regular.h>

#include "hangul_ime.h"

// Font objects (file-scoped)
static EpdFont rb16(&ridibatang_16_regular);
static EpdFont rb14(&ridibatang_14_regular);
static EpdFont rb12(&ridibatang_12_regular);
static EpdFontFamily rb16Family(&rb16);
static EpdFontFamily rb14Family(&rb14);
static EpdFontFamily rb12Family(&rb12);

// UI fonts take Hangul from RIDIBatang of the nearest size.
static EpdFont ns16Regular(&notosans_16_regular);
static EpdFont ns16Bold(&notosans_16_bold);
static EpdFontFamily ns16Family(&ns16Regular, &ns16Bold, nullptr, nullptr, &rb16);

static EpdFont ns14Regular(&notosans_14_regular);
static EpdFont ns14Bold(&notosans_14_bold);
static EpdFontFamily ns14Family(&ns14Regular, &ns14Bold, nullptr, nullptr, &rb14);

static EpdFont ns12Regular(&notosans_12_regular);
static EpdFont ns12Bold(&notosans_12_bold);
static EpdFontFamily ns12Family(&ns12Regular, &ns12Bold, nullptr, nullptr, &rb12);

static EpdFont u10Regular(&ubuntu_10_regular);
static EpdFont u10Bold(&ubuntu_10_bold);
static EpdFontFamily u10Family(&u10Regular, &u10Bold, nullptr, nullptr, &rb12);

// OTA app detection (defined in main.cpp)
extern OtaAppEntry otaApps[];
extern int otaAppCount;

// Extern shared state (defined in main.cpp)
extern UIState currentState;
extern int mainMenuSelection;
extern int selectedFileIndex;
extern int settingsSelection;
extern int bluetoothDeviceSelection;
extern bool btShowAllDevices;
extern int pairedKeyboardSelection;
extern bool pairedForgetPending;
extern Orientation currentOrientation;
extern int charsPerLine;
extern char renameBuffer[];
extern int renameBufferLen;
extern bool editorFindMode;
extern char findBuffer[];

void rendererSetup(GfxRenderer& renderer) {
  renderer.insertFont(FONT_LARGE, ns16Family);
  renderer.insertFont(FONT_BODY, ns14Family);
  renderer.insertFont(FONT_UI, ns12Family);
  renderer.insertFont(FONT_SMALL, u10Family);
  renderer.insertFont(FONT_EDITOR_SMALL, rb12Family);
  renderer.insertFont(FONT_EDITOR_MEDIUM, rb14Family);
  renderer.insertFont(FONT_EDITOR_LARGE, rb16Family);
}

// RIDIBatang's own line height is 1.0 em, too tight for Hangul; the editor
// adds 40% leading and centres the text in the taller line.
static int editorLineHeight(GfxRenderer& renderer) {
  return renderer.getLineHeight(editorFontId(fontSize)) * 7 / 5;
}

// ---------------------------------------------------------------------------
// Clipped draw helpers — use renderer.truncatedText() so NO pixel ever
// exceeds screen width.  This is how crosspoint-reader prevents GFX errors.
// ---------------------------------------------------------------------------

// Draw text that is guaranteed not to overflow the screen width.
// maxW = available pixel width from x to right edge (caller computes).
// Falls back to sw - x - 5 if maxW <= 0.
static void drawClippedText(GfxRenderer& r, int font, int x, int y,
                            const char* text, int maxW = 0,
                            bool black = true,
                            EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  if (!text || !text[0]) return;
  int sw = r.getScreenWidth();
  int sh = r.getScreenHeight();
  if (x < 0 || x >= sw || y < 0 || y >= sh) return;

  if (maxW <= 0) maxW = sw - x - 5;   // 5px right margin
  if (maxW <= 0) return;

  auto clipped = r.truncatedText(font, text, maxW, style);
  if (!clipped.empty()) {
    r.drawText(font, x, y, clipped.c_str(), black, style);
  }
}

// Draw right-aligned text (e.g. battery %, RSSI, settings values).
// Computes its own X from the measured text width.
static void drawRightText(GfxRenderer& r, int font, int rightEdge, int y,
                          const char* text, bool black = true,
                          EpdFontFamily::Style style = EpdFontFamily::REGULAR) {
  if (!text || !text[0]) return;
  // Use getTextWidth (bounding box) — same measurement truncatedText uses —
  // so the allocated space always matches what the truncation check expects.
  int tw = r.getTextWidth(font, text, style);
  if (tw <= 0) tw = 30;                    // safe fallback
  int x = rightEdge - tw;
  if (x < 5) x = 5;                        // don't go off left edge
  drawClippedText(r, font, x, y, text, rightEdge - x, black, style);
}

// Safe line — just clamp to screen
static void clippedLine(GfxRenderer& r, int x1, int y1, int x2, int y2,
                        bool state = true) {
  int sw = r.getScreenWidth();
  int sh = r.getScreenHeight();
  // Clamp rather than reject
  auto clamp = [](int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); };
  x1 = clamp(x1, 0, sw - 1);
  x2 = clamp(x2, 0, sw - 1);
  y1 = clamp(y1, 0, sh - 1);
  y2 = clamp(y2, 0, sh - 1);
  r.drawLine(x1, y1, x2, y2, state);
}

// Safe fillRect — clamp dimensions to screen
static void clippedFillRect(GfxRenderer& r, int x, int y, int w, int h,
                            bool state = true) {
  int sw = r.getScreenWidth();
  int sh = r.getScreenHeight();
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > sw) w = sw - x;
  if (y + h > sh) h = sh - y;
  if (w > 0 && h > 0) r.fillRect(x, y, w, h, state);
}

// ---------------------------------------------------------------------------
// Helper: draw battery percentage in top-right
// ---------------------------------------------------------------------------
static void drawBattery(GfxRenderer& renderer, HalGPIO& gpio) {
  int pct = gpio.getBatteryPercentage();
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  drawRightText(renderer, FONT_SMALL, renderer.getScreenWidth() - 8, 5, buf, !darkMode);
}

// Helper: draw BLE status
static void drawBleStatus(GfxRenderer& renderer, int x, int y) {
  const char* status = "";
  switch (getConnectionState()) {
    case BLEState::CONNECTED:    status = tr(StrId::BLE_KB_CONNECTED); break;
    case BLEState::SCANNING:     status = tr(StrId::SCANNING); break;
    case BLEState::CONNECTING:   status = tr(StrId::CONNECTING); break;
    case BLEState::DISCONNECTED: status = tr(StrId::BLE_KB_DISCONNECTED); break;
  }
  drawClippedText(renderer, FONT_SMALL, x, y, status, 0, !darkMode);
}

// ===========================================================================
// Screen drawing functions
// ===========================================================================

void drawMainMenu(GfxRenderer& renderer, HalGPIO& gpio) {
  renderer.clearScreen();
  int sw = renderer.getScreenWidth();
  int sh = renderer.getScreenHeight();
  bool tc = !darkMode;  // text color

  if (darkMode) clippedFillRect(renderer, 0, 0, sw, sh, true);

  // Title
  renderer.drawCenteredText(FONT_BODY, 30, "MicroSlate", tc, EpdFontFamily::BOLD);

  // Menu items (base + dynamically detected OTA apps)
  const char* baseMenuItems[] = {tr(StrId::MENU_BROWSE), tr(StrId::MENU_NEW_NOTE),
                                 tr(StrId::MENU_SETTINGS), tr(StrId::MENU_SYNC)};
  int menuCount = 4 + otaAppCount;
  for (int i = 0; i < menuCount; i++) {
    int yPos = 90 + (i * 45);
    const char* label = (i < 4) ? baseMenuItems[i] : otaApps[i - 4].name;
    if (i == mainMenuSelection) {
      clippedFillRect(renderer, 5, yPos - 5, sw - 10, 35, tc);
      drawClippedText(renderer, FONT_UI, 20, yPos, label, sw - 40, !tc);
    } else {
      drawClippedText(renderer, FONT_UI, 20, yPos, label, sw - 40, tc);
    }
  }

  // Footer
  constexpr int bm = 60;
  if (sh > bm + 40) {
    clippedLine(renderer, 10, sh - bm, sw - 10, sh - bm, tc);
    drawClippedText(renderer, FONT_SMALL, 20, sh - bm + 12, tr(StrId::HINT_MAIN), 0, tc);
    drawBleStatus(renderer, 20, sh - bm + 28);
  }
  drawBattery(renderer, gpio);

  renderer.beginRefresh(HalDisplay::FAST_REFRESH);
}

void drawFileBrowser(GfxRenderer& renderer, HalGPIO& gpio) {
  renderer.clearScreen();
  int sw = renderer.getScreenWidth();
  int sh = renderer.getScreenHeight();
  bool tc = !darkMode;

  if (darkMode) clippedFillRect(renderer, 0, 0, sw, sh, true);

  // Header
  drawClippedText(renderer, FONT_SMALL, 10, 5, tr(StrId::TITLE_NOTES), 0, tc, EpdFontFamily::BOLD);
  drawBattery(renderer, gpio);
  clippedLine(renderer, 5, 32, sw - 5, 32, tc);

  int fc = getFileCount();
  int lineH = 30;
  int listTop = 42;
  int footerH = 28;  // one line of FONT_SMALL with safe bottom margin
  int maxVisible = (sh - listTop - footerH) / lineH;
  int startIdx = 0;
  if (fc > maxVisible && selectedFileIndex >= maxVisible) {
    startIdx = selectedFileIndex - maxVisible + 1;
  }

  if (fc == 0) {
    drawClippedText(renderer, FONT_UI, 20, listTop + 14, tr(StrId::NO_NOTES), 0, tc);
    drawClippedText(renderer, FONT_SMALL, 20, listTop + 36, tr(StrId::HINT_CREATE_NOTE), 0, tc);
  }

  FileInfo* files = getFileList();
  for (int i = startIdx; i < fc && (i - startIdx) < maxVisible; i++) {
    int yPos = listTop + (i - startIdx) * lineH;

    if (i == selectedFileIndex) {
      clippedFillRect(renderer, 5, yPos - 3, sw - 10, lineH - 1, tc);
      drawClippedText(renderer, FONT_UI, 15, yPos, files[i].title, sw - 30, !tc);
    } else {
      drawClippedText(renderer, FONT_UI, 15, yPos, files[i].title, sw - 30, tc);
    }
  }

  // Footer
  clippedLine(renderer, 5, sh - footerH - 2, sw - 5, sh - footerH - 2, tc);
  if (deleteConfirmPending && fc > 0) {
    drawClippedText(renderer, FONT_SMALL, 10, sh - footerH + 4, tr(StrId::DELETE_CONFIRM), 0, tc);
  } else {
    drawClippedText(renderer, FONT_SMALL, 10, sh - footerH + 4,
                    tr(StrId::HINT_BROWSER), 0, tc);
  }

  renderer.beginRefresh(HalDisplay::FAST_REFRESH);
}

// Helper: draw a single editor line from the buffer
static void drawEditorLine(GfxRenderer& renderer, int lineIdx, int x, int yPos,
                           int lineHeight, int maxW, bool tc) {
  char* buf = editorGetBuffer();
  size_t bufLen = editorGetLength();
  int totalLines = editorGetLineCount();

  int lineStart = editorGetLinePosition(lineIdx);
  int lineEnd = (lineIdx + 1 < totalLines) ? editorGetLinePosition(lineIdx + 1) : (int)bufLen;
  int dispEnd = lineEnd;
  if (dispEnd > lineStart && buf[dispEnd - 1] == '\n') dispEnd--;

  int len = dispEnd - lineStart;
  if (len <= 0) return;

  char lineBuf[512];
  int copyLen = (len < (int)sizeof(lineBuf) - 1) ? len : (int)sizeof(lineBuf) - 1;
  strncpy(lineBuf, buf + lineStart, copyLen);
  lineBuf[copyLen] = '\0';

  int fontId = editorFontId(fontSize);
  const int textY = yPos + (lineHeight - renderer.getLineHeight(fontId)) / 2;

  // Draw the line normally first.
  drawClippedText(renderer, fontId, x, textY, lineBuf, maxW, tc);

  // Overlay the selection on this line as inverse video, if any overlaps it.
  int selLo, selHi;
  if (editorGetSelectionRange(&selLo, &selHi)) {
    int a = (selLo > lineStart) ? selLo : lineStart;       // clamp to line
    int b = (selHi < dispEnd)  ? selHi : dispEnd;
    int aOff = a - lineStart;
    int bOff = b - lineStart;
    if (aOff < 0) aOff = 0;
    if (bOff > copyLen) bOff = copyLen;
    if (bOff > aOff) {
      char pre[512], sel[512];
      strncpy(pre, lineBuf, aOff); pre[aOff] = '\0';
      int selLen = bOff - aOff;
      strncpy(sel, lineBuf + aOff, selLen); sel[selLen] = '\0';
      int xA = x + renderer.getTextAdvanceX(fontId, pre);
      int selW = renderer.getTextAdvanceX(fontId, sel);
      // Solid block in the text colour, then the selected text in the background
      // colour on top → inverse video.
      clippedFillRect(renderer, xA, yPos, selW, lineHeight, tc);
      drawClippedText(renderer, fontId, xA, textY, sel, maxW - (xA - x), !tc);
    }
  }
}

// Helper: draw cursor at the given screen position
static void drawEditorCursor(GfxRenderer& renderer, int cursorY, int lineHeight,
                             int sw, bool tc) {
  int curLine = editorGetCursorLine();
  int curPos = editorGetCursorPosition();
  char* buf = editorGetBuffer();

  int lineStart = editorGetLinePosition(curLine);
  char prefix[512];
  int prefixLen = curPos - lineStart;
  if (prefixLen < 0) prefixLen = 0;
  if (prefixLen > (int)sizeof(prefix) - 1) prefixLen = (int)sizeof(prefix) - 1;
  while (prefixLen > 0 && ((static_cast<unsigned char>(buf[lineStart + prefixLen]) & 0xC0) == 0x80)) {
    prefixLen--;
  }
  strncpy(prefix, buf + lineStart, prefixLen);
  prefix[prefixLen] = '\0';

  int cursorX = 10 + renderer.getTextAdvanceX(editorFontId(fontSize), prefix);
  int cursorW = renderer.getSpaceWidth(editorFontId(fontSize));
  if (cursorW < 2) cursorW = 8;

  if (cursorX >= 0 && cursorX + cursorW <= sw && cursorY >= 0 && cursorY + lineHeight <= renderer.getScreenHeight()) {
    renderer.fillRect(cursorX, cursorY, cursorW, lineHeight, tc);
  }

  // Underline the Hangul syllable still being composed (it sits just before the cursor).
  if (hangulIsComposing() && !editorFindMode && prefixLen > 0) {
    int lastStart = prefixLen - 1;
    while (lastStart > 0 && (static_cast<unsigned char>(prefix[lastStart]) & 0xC0) == 0x80) lastStart--;
    const int preeditX = cursorX - renderer.getTextAdvanceX(editorFontId(fontSize), prefix + lastStart);
    const int underlineY = cursorY + lineHeight - 3;
    if (preeditX >= 0 && cursorX <= sw && underlineY + 2 <= renderer.getScreenHeight()) {
      renderer.fillRect(preeditX, underlineY, cursorX - preeditX, 2, tc);
    }
  }
}

// Get the mode indicator string for the current writing mode
static const char* getModeIndicator() {
  switch (writingMode) {
    case WritingMode::TYPEWRITER: return "[T]";
    case WritingMode::PAGINATION: return "[P]";
    default:                      return "[S]";
  }
}

// Helper: draw the standard editor header, returns textAreaTop
// centerText is optional text drawn centered in the header (e.g. "Page 1/3")
static int drawEditorHeader(GfxRenderer& renderer, HalGPIO& gpio, int sw, bool tc,
                            const char* centerText = nullptr) {
  if (cleanMode) return 8;

  const char* title = editorGetCurrentTitle();
  char headerBuf[64];
  if (editorHasUnsavedChanges()) {
    snprintf(headerBuf, sizeof(headerBuf), "%s *", title);
  } else {
    strncpy(headerBuf, title, sizeof(headerBuf) - 1);
    headerBuf[sizeof(headerBuf) - 1] = '\0';
  }
  // Mode indicator — fixed position, right-anchored before battery
  const char* modeInd = getModeIndicator();
  int modeW = renderer.getTextAdvanceX(FONT_SMALL, modeInd);
  int modeX = sw - 70 - modeW;
  drawClippedText(renderer, FONT_SMALL, modeX, 5, modeInd, modeW + 5, tc);

  // Layout indicator — shown to the left of the mode indicator when non-QWERTY
  // so the user always knows which layout Ctrl+K will toggle away from.
  int layoutIndX = modeX - 8;
  if (currentKeyboardLayout != KeyboardLayout::QWERTY) {
    const char* layoutInd = keyboardLayoutShortName(currentKeyboardLayout);
    int layoutW = renderer.getTextAdvanceX(FONT_SMALL, layoutInd);
    layoutIndX = modeX - 8 - layoutW;
    if (layoutIndX > 10)
      drawClippedText(renderer, FONT_SMALL, layoutIndX, 5, layoutInd, layoutW + 5, tc);
  }

  // Word count — drawn to the left of the layout indicator (or mode indicator)
  int titleMaxW = layoutIndX - 10;
  if (showWordCount) {
    int wc = editorGetWordCount();
    char wcBuf[32];
    if (wc == 1) snprintf(wcBuf, sizeof(wcBuf), "%s", tr(StrId::WORD_ONE));
    else         snprintf(wcBuf, sizeof(wcBuf), tr(StrId::WORDS_FMT), wc);
    int wcW = renderer.getTextAdvanceX(FONT_SMALL, wcBuf);
    int wcX = layoutIndX - 8 - wcW;
    if (wcX > 10) {
      drawClippedText(renderer, FONT_SMALL, wcX, 5, wcBuf, wcW + 5, tc);
      titleMaxW = wcX - 10;
    }
  }

  // Title — stops before word count (or mode indicator if word count hidden)
  drawClippedText(renderer, FONT_SMALL, 10, 5, headerBuf, titleMaxW, tc, EpdFontFamily::BOLD);

  // Centered text (e.g. page indicator)
  if (centerText) {
    int ctW = renderer.getTextAdvanceX(FONT_SMALL, centerText);
    drawClippedText(renderer, FONT_SMALL, (sw - ctW) / 2, 5, centerText, ctW + 5, tc);
  }

  drawBattery(renderer, gpio);
  clippedLine(renderer, 5, 32, sw - 5, 32, tc);
  return 38;
}

// Bottom overlay bar showing the current find query. Drawn over the editor while
// find mode is active (the matched text itself highlights via the selection).
static void drawFindBar(GfxRenderer& renderer, int sw, int sh, bool tc) {
  int barH = renderer.getLineHeight(FONT_UI) + 10;
  int barY = sh - barH;
  clippedFillRect(renderer, 0, barY, sw, barH, !tc);   // clear to background
  clippedLine(renderer, 0, barY, sw, barY, tc);        // top divider
  char line[72];
  snprintf(line, sizeof(line), tr(StrId::FIND_FMT), findBuffer);
  drawClippedText(renderer, FONT_UI, 8, barY + 5, line, sw - 100, tc);
  drawRightText(renderer, FONT_SMALL, sw - 8, barY + 7, tr(StrId::HINT_FIND), tc);
}

void drawTextEditor(GfxRenderer& renderer, HalGPIO& gpio) {
  renderer.clearScreen();
  int sw = renderer.getScreenWidth();
  int sh = renderer.getScreenHeight();
  bool tc = !darkMode;

  if (darkMode) clippedFillRect(renderer, 0, 0, sw, sh, true);

  int lineHeight = editorLineHeight(renderer);
  if (lineHeight <= 0) lineHeight = 20;
  int totalLines = editorGetLineCount();
  int curLine = editorGetCursorLine();

  // --- TYPEWRITER MODE ---
  if (writingMode == WritingMode::TYPEWRITER) {
    // In clean mode (Ctrl+Z): just text on blank screen, no header
    int textAreaTop = cleanMode ? 0 : drawEditorHeader(renderer, gpio, sw, tc);

    // Center the current line vertically
    int textAreaHeight = sh - textAreaTop;
    int centerY = textAreaTop + (textAreaHeight / 2) - (lineHeight / 2);

    // Draw only the current line
    if (curLine < totalLines) {
      drawEditorLine(renderer, curLine, 10, centerY, lineHeight, sw - 20, tc);
    }

    // Draw cursor
    drawEditorCursor(renderer, centerY, lineHeight, sw, tc);

    editorSetVisibleLines(1);

    if (editorFindMode) drawFindBar(renderer, sw, sh, tc);
    renderer.beginRefresh(HalDisplay::FAST_REFRESH);
    return;
  }

  // --- PAGINATION MODE ---
  if (writingMode == WritingMode::PAGINATION) {
    // Pre-compute page info for the header
    // Use a temporary linesPerPage estimate (will be exact since header height is fixed)
    int tempTextTop = cleanMode ? 8 : 38;
    int tempLinesPerPage = (sh - 5 - tempTextTop) / lineHeight;
    if (tempLinesPerPage < 1) tempLinesPerPage = 1;
    int currentPage = curLine / tempLinesPerPage;
    int totalPages = (totalLines + tempLinesPerPage - 1) / tempLinesPerPage;
    if (totalPages < 1) totalPages = 1;

    char pageStr[24];
    snprintf(pageStr, sizeof(pageStr), tr(StrId::PAGE_FMT), currentPage + 1, totalPages);
    int textAreaTop = drawEditorHeader(renderer, gpio, sw, tc, pageStr);

    int textAreaBottom = sh - 5;
    int textAreaHeight = textAreaBottom - textAreaTop;
    int linesPerPage = textAreaHeight / lineHeight;
    if (linesPerPage < 1) linesPerPage = 1;

    // Recompute with actual linesPerPage if it differs
    currentPage = curLine / linesPerPage;
    int pageStart = currentPage * linesPerPage;

    editorSetVisibleLines(linesPerPage);

    // Draw lines for this page
    for (int i = 0; i < linesPerPage && (pageStart + i) < totalLines; i++) {
      int yPos = textAreaTop + (i * lineHeight);
      drawEditorLine(renderer, pageStart + i, 10, yPos, lineHeight, sw - 20, tc);
    }

    // Draw cursor if on this page
    if (curLine >= pageStart && curLine < pageStart + linesPerPage) {
      int cursorY = textAreaTop + ((curLine - pageStart) * lineHeight);
      drawEditorCursor(renderer, cursorY, lineHeight, sw, tc);
    }

    if (editorFindMode) drawFindBar(renderer, sw, sh, tc);
    renderer.beginRefresh(HalDisplay::FAST_REFRESH);
    return;
  }

  // --- NORMAL MODE ---
  int textAreaTop = drawEditorHeader(renderer, gpio, sw, tc);

  int textAreaBottom = sh - 5;
  int textAreaHeight = textAreaBottom - textAreaTop;
  int visibleLines = textAreaHeight / lineHeight;

  editorSetVisibleLines(visibleLines);

  int vpStart = editorGetViewportStart();

  // Draw visible lines
  for (int i = 0; i < visibleLines && (vpStart + i) < totalLines; i++) {
    int yPos = textAreaTop + (i * lineHeight);
    drawEditorLine(renderer, vpStart + i, 10, yPos, lineHeight, sw - 20, tc);
  }

  // Draw cursor
  if (curLine >= vpStart && curLine < vpStart + visibleLines) {
    int cursorY = textAreaTop + ((curLine - vpStart) * lineHeight);
    drawEditorCursor(renderer, cursorY, lineHeight, sw, tc);
  }

  if (editorFindMode) drawFindBar(renderer, sw, sh, tc);
  renderer.beginRefresh(HalDisplay::FAST_REFRESH);
}

void drawRenameScreen(GfxRenderer& renderer, HalGPIO& gpio) {
  renderer.clearScreen();
  int sw = renderer.getScreenWidth();
  int sh = renderer.getScreenHeight();
  bool tc = !darkMode;

  if (darkMode) clippedFillRect(renderer, 0, 0, sw, sh, true);

  drawClippedText(renderer, FONT_SMALL, 10, 5, tr(StrId::TITLE_EDIT), 0, tc, EpdFontFamily::BOLD);
  drawBattery(renderer, gpio);
  clippedLine(renderer, 5, 32, sw - 5, 32, tc);

  drawClippedText(renderer, FONT_SMALL, 20, 42, tr(StrId::NOTE_TITLE_LABEL), 0, tc);
  // Auto-size the input box to the text line height with even padding, so the
  // text and caret are vertically centred and never touch the border.
  int lineH = renderer.getLineHeight(FONT_UI);
  if (lineH <= 0) lineH = 18;
  constexpr int pad = 6;
  int boxY = 64;
  int boxH = lineH + pad * 2;
  int textY = boxY + pad;
  renderer.drawRect(15, boxY, sw - 30, boxH, tc);
  drawClippedText(renderer, FONT_UI, 20, textY, renameBuffer, sw - 50, tc);

  // Caret — same height as the text line, inset by the box padding
  int cursorX = 20 + renderer.getTextAdvanceX(FONT_UI, renameBuffer);
  if (cursorX + 2 < sw - 15)
    renderer.fillRect(cursorX, textY, 2, lineH, tc);

  drawClippedText(renderer, FONT_SMALL, 20, boxY + boxH + 8,
                  tr(StrId::TITLE_QWERTY_NOTE), sw - 40, tc);

  // Footer
  clippedLine(renderer, 5, sh - 36, sw - 5, sh - 36, tc);
  drawClippedText(renderer, FONT_SMALL, 10, sh - 30, tr(StrId::HINT_CONFIRM), 0, tc);

  renderer.beginRefresh(HalDisplay::FAST_REFRESH);
}

void drawSettingsMenu(GfxRenderer& renderer, HalGPIO& gpio) {
  renderer.clearScreen();
  int sw = renderer.getScreenWidth();
  int sh = renderer.getScreenHeight();

  if (darkMode) clippedFillRect(renderer, 0, 0, sw, sh, true);

  drawClippedText(renderer, FONT_SMALL, 10, 5, tr(StrId::TITLE_SETTINGS), 0, !darkMode, EpdFontFamily::BOLD);
  drawBattery(renderer, gpio);
  clippedLine(renderer, 5, 32, sw - 5, 32, !darkMode);

  // Setting items: Orientation, Dark Mode, Writing Mode, Font Size, Layout, BLE Power, Bluetooth, Paired Keyboards, Language
  const char* labels[] = {
    tr(StrId::SET_ORIENTATION), tr(StrId::SET_DARK_MODE), tr(StrId::SET_WRITING_MODE), tr(StrId::SET_FONT_SIZE),
    tr(StrId::SET_KB_LAYOUT), tr(StrId::SET_BLE_POWER), tr(StrId::SET_BLUETOOTH), tr(StrId::SET_PAIRED),
    tr(StrId::SET_LANGUAGE)
  };
  const int SETTINGS_COUNT = 9;

  // Compute line height to fit all items — use smaller spacing if needed
  int lineH = 38;
  int listTop = 50;
  if (listTop + SETTINGS_COUNT * lineH > sh - 70) {
    lineH = (sh - 70 - listTop) / SETTINGS_COUNT;
    if (lineH < 24) lineH = 24;
  }

  for (int i = 0; i < SETTINGS_COUNT; i++) {
    int yPos = listTop + (i * lineH);
    bool sel = (i == settingsSelection);

    if (sel) {
      clippedFillRect(renderer, 5, yPos - 5, sw - 10, lineH - 6, !darkMode);
      drawClippedText(renderer, FONT_UI, 15, yPos, labels[i], sw / 2 - 15, darkMode);
    } else {
      drawClippedText(renderer, FONT_UI, 15, yPos, labels[i], sw / 2 - 15, !darkMode);
    }

    // Value on the right
    char val[40] = "";
    if (i == 0) {
      switch (currentOrientation) {
        case Orientation::PORTRAIT:      strcpy(val, tr(StrId::ORIENT_PORTRAIT)); break;
        case Orientation::LANDSCAPE_CW:  strcpy(val, tr(StrId::ORIENT_LAND_CW)); break;
        case Orientation::PORTRAIT_INV:  strcpy(val, tr(StrId::ORIENT_INVERTED)); break;
        case Orientation::LANDSCAPE_CCW: strcpy(val, tr(StrId::ORIENT_LAND_CCW)); break;
      }
    } else if (i == 1) {
      strcpy(val, tr(darkMode ? StrId::DARK : StrId::LIGHT));
    } else if (i == 2) {
      switch (writingMode) {
        case WritingMode::NORMAL:     strcpy(val, tr(StrId::MODE_NORMAL)); break;
        case WritingMode::TYPEWRITER: strcpy(val, tr(StrId::MODE_TYPEWRITER)); break;
        case WritingMode::PAGINATION: strcpy(val, tr(StrId::MODE_PAGINATION)); break;
      }
    } else if (i == 3) {
      switch (fontSize) {
        case FontSize::SMALL:  strcpy(val, tr(StrId::SIZE_SMALL)); break;
        case FontSize::MEDIUM: strcpy(val, tr(StrId::SIZE_MEDIUM)); break;
        default:               strcpy(val, tr(StrId::SIZE_LARGE)); break;
      }
    } else if (i == 4) {
      strncpy(val, keyboardLayoutName(currentKeyboardLayout), sizeof(val) - 1);
      val[sizeof(val) - 1] = '\0';
    } else if (i == 5) {
      strncpy(val, getBleTxPowerLabel(), sizeof(val) - 1);
      val[sizeof(val) - 1] = '\0';
    } else if (i == 7) {
      int kbCount = getPairedKeyboardCount();
      if (kbCount == 0) strcpy(val, tr(StrId::NONE));
      else if (kbCount == 1) strcpy(val, tr(StrId::ONE_KEYBOARD));
      else snprintf(val, sizeof(val), tr(StrId::KEYBOARDS_COUNT_FMT), kbCount);
    } else if (i == 8) {
      strcpy(val, languageName(getLanguage()));
    }

    if (val[0] != '\0') {
      drawRightText(renderer, FONT_UI, sw - 20, yPos, val, sel ? darkMode : !darkMode);
    }
  }

  // Footer
  constexpr int bm = 60;
  if (sh > bm + 30) {
    clippedLine(renderer, 10, sh - bm, sw - 10, sh - bm, !darkMode);
    drawClippedText(renderer, FONT_SMALL, 20, sh - bm + 12,
                    tr(StrId::HINT_SETTINGS), 0, !darkMode);
  }

  renderer.beginRefresh(HalDisplay::FAST_REFRESH);
}

void drawBluetoothSettings(GfxRenderer& renderer, HalGPIO& gpio) {
  int sw = renderer.getScreenWidth();
  int sh = renderer.getScreenHeight();

  renderer.clearScreen();
  bool tc = !darkMode;

  if (darkMode) clippedFillRect(renderer, 0, 0, sw, sh, true);

  // Header
  drawClippedText(renderer, FONT_SMALL, 10, 5, tr(StrId::TITLE_BT), 0, tc, EpdFontFamily::BOLD);
  drawBattery(renderer, gpio);
  clippedLine(renderer, 5, 32, sw - 5, 32, tc);

  // Connection status
  const char* status = "";
  switch (getConnectionState()) {
    case BLEState::CONNECTED:    status = tr(StrId::BT_CONNECTED); break;
    case BLEState::SCANNING:     status = tr(StrId::BT_SCANNING); break;
    case BLEState::CONNECTING:   status = tr(StrId::CONNECTING); break;
    case BLEState::DISCONNECTED: status = tr(StrId::BT_NOT_CONNECTED); break;
  }
  drawClippedText(renderer, FONT_SMALL, 10, 45, status, sw / 2 - 10, tc);

  // Paired device info
  std::string storedAddr, storedName;
  if (getStoredDevice(storedAddr, storedName)) {
    char pairedStr[64];
    snprintf(pairedStr, sizeof(pairedStr), tr(StrId::PAIRED_FMT), storedName.c_str());
    drawClippedText(renderer, FONT_SMALL, sw / 2, 45, pairedStr, sw / 2 - 10, tc);
  }

  // Passkey display
  uint32_t passkey = getCurrentPasskey();
  if (passkey > 0) {
    char passkeyStr[32];
    drawClippedText(renderer, FONT_UI, 20, 100, tr(StrId::PAIRING_CODE), 0, tc, EpdFontFamily::BOLD);
    snprintf(passkeyStr, sizeof(passkeyStr), "%06lu", passkey);
    drawClippedText(renderer, FONT_BODY, 20, 130, passkeyStr, 0, tc, EpdFontFamily::BOLD);
    drawClippedText(renderer, FONT_SMALL, 20, 160, tr(StrId::TYPE_CODE_1), 0, tc);
    drawClippedText(renderer, FONT_SMALL, 20, 180, tr(StrId::TYPE_CODE_2), 0, tc);
  } else if (isDeviceScanning()) {
    static uint8_t dotPhase = 0;
    static uint32_t lastAnimMs = 0;
    if (millis() - lastAnimMs > 900) {
      dotPhase = (dotPhase + 1) % 4;
      lastAnimMs = millis();
    }
    std::string dots(dotPhase, '.');
    char scanningStr[64];
    int deviceCount = getDiscoveredDeviceCount();
    snprintf(scanningStr, sizeof(scanningStr), tr(StrId::SEARCHING_FMT), dots.c_str());
    drawClippedText(renderer, FONT_SMALL, 10, 68, scanningStr, sw / 2 - 10, tc);

    char foundStr[32];
    snprintf(foundStr, sizeof(foundStr), tr(StrId::FOUND_FMT), deviceCount);
    drawClippedText(renderer, FONT_SMALL, sw / 2, 68, foundStr, sw / 2 - 10, tc);
  }

  // Device list — keyboards first; non-keyboards hidden behind a toggle row.
  int visIdx[32];
  int total = 0, kbd = 0;
  int visCount = getVisibleDeviceIndices(btShowAllDevices, visIdx, 32, &total, &kbd);
  bool toggleShown = (kbd > 0 && total > kbd);
  int itemCount = visCount + (toggleShown ? 1 : 0);

  if (total > 0) {
    BleDeviceInfo* devices = getDiscoveredDevices();

    // Count header (hidden while scanning to avoid colliding with "Found: N").
    if (!isDeviceScanning()) {
      char headerStr[64];
      if (btShowAllDevices || kbd == 0)
        snprintf(headerStr, sizeof(headerStr), tr(StrId::DEVICES_FMT), total);
      else
        snprintf(headerStr, sizeof(headerStr), tr(StrId::KEYBOARDS_FMT), visCount);
      drawClippedText(renderer, FONT_SMALL, 10, 68, headerStr, 0, tc, EpdFontFamily::BOLD);
    }

    int sel = bluetoothDeviceSelection;
    if (sel >= itemCount) sel = itemCount - 1;
    if (sel < 0) sel = 0;

    // Show up to 10 rows (pagination via scrolling)
    int maxToShow = 10;
    int startIndex = (sel >= maxToShow) ? sel - maxToShow + 1 : 0;
    int itemsToShow = (itemCount - startIndex < maxToShow) ? itemCount - startIndex : maxToShow;

    for (int i = 0; i < itemsToShow; i++) {
      int pos = startIndex + i;
      int yPos = 98 + (i * 30);
      if (yPos > sh - 100) break;   // don't draw into the footer zone

      bool isSel = (sel == pos);

      if (pos < visCount) {
        int d = visIdx[pos];
        bool isConnected = (getCurrentDeviceAddress() == devices[d].address);
        const char* displayName = devices[d].name.empty()
                                  ? devices[d].address.c_str()
                                  : devices[d].name.c_str();
        int nameMaxW = sw - 100;   // leave room for RSSI
        if (isSel || isConnected) {
          clippedFillRect(renderer, 5, yPos - 5, sw - 10, 25, tc);
          drawClippedText(renderer, FONT_UI, 15, yPos, displayName, nameMaxW, !tc);
        } else {
          drawClippedText(renderer, FONT_UI, 15, yPos, displayName, nameMaxW, tc);
        }
        char rssiStr[16];
        snprintf(rssiStr, sizeof(rssiStr), "%ddBm", devices[d].rssi);
        drawRightText(renderer, FONT_SMALL, sw - 10, yPos, rssiStr, (isSel || isConnected) ? !tc : tc);
      } else {
        // The "Show other devices" / "Show keyboards only" toggle row
        char toggleStr[40];
        if (btShowAllDevices)
          snprintf(toggleStr, sizeof(toggleStr), "%s", tr(StrId::SHOW_KB_ONLY));
        else
          snprintf(toggleStr, sizeof(toggleStr), tr(StrId::SHOW_OTHER_FMT), total - kbd);
        if (isSel) {
          clippedFillRect(renderer, 5, yPos - 5, sw - 10, 25, tc);
          drawClippedText(renderer, FONT_UI, 15, yPos, toggleStr, sw - 30, !tc);
        } else {
          drawClippedText(renderer, FONT_UI, 15, yPos, toggleStr, sw - 30, tc);
        }
      }
    }

    // Page indicator
    if (itemCount > maxToShow) {
      char navHint[32];
      int pageNum = (sel / maxToShow) + 1;
      int totalPages = (itemCount + maxToShow - 1) / maxToShow;
      snprintf(navHint, sizeof(navHint), tr(StrId::PAGE_NUM_FMT), pageNum, totalPages);
      int navY = 98 + (itemsToShow * 30);
      if (navY < sh - 100)
        drawClippedText(renderer, FONT_SMALL, 15, navY, navHint, 0, tc);
    }
  } else {
    drawClippedText(renderer, FONT_UI, 20, 80, tr(StrId::NO_DEVICES), 0, tc);
    drawClippedText(renderer, FONT_SMALL, 20, 100, tr(StrId::HINT_SCAN_ENTER), 0, tc);
  }

  // Footer
  constexpr int bm = 62;
  if (sh > bm + 30) {
    clippedLine(renderer, 10, sh - bm, sw - 10, sh - bm, tc);
    drawClippedText(renderer, FONT_SMALL, 10, sh - bm + 10, tr(StrId::HINT_BT_1), 0, tc);
    drawClippedText(renderer, FONT_SMALL, 10, sh - bm + 32, tr(StrId::HINT_BT_2), 0, tc);
  }

  renderer.beginRefresh(HalDisplay::FAST_REFRESH);
}

void drawPairedKeyboardsMenu(GfxRenderer& renderer, HalGPIO& gpio) {
  renderer.clearScreen();
  int sw = renderer.getScreenWidth();
  int sh = renderer.getScreenHeight();
  bool tc = !darkMode;

  if (darkMode) clippedFillRect(renderer, 0, 0, sw, sh, true);

  drawClippedText(renderer, FONT_SMALL, 10, 5, tr(StrId::TITLE_PAIRED), 0, tc, EpdFontFamily::BOLD);
  drawBattery(renderer, gpio);
  clippedLine(renderer, 5, 32, sw - 5, 32, tc);

  int count = getPairedKeyboardCount();
  if (count == 0) {
    drawClippedText(renderer, FONT_UI, 20, 60, tr(StrId::NO_PAIRED), 0, tc);
    drawClippedText(renderer, FONT_SMALL, 20, 85, tr(StrId::HINT_GO_BT), 0, tc);
  } else {
    std::string currentAddr = getCurrentDeviceAddress();
    int lineH = 38;
    int listTop = 44;

    for (int i = 0; i < count; i++) {
      std::string addr, name; uint8_t addrType;
      getPairedKeyboard(i, addr, name, addrType);

      int yPos = listTop + (i * lineH);
      bool sel = (i == pairedKeyboardSelection);
      bool active = (!currentAddr.empty() && currentAddr == addr);

      if (sel) {
        clippedFillRect(renderer, 5, yPos - 4, sw - 10, lineH - 4, tc);
        drawClippedText(renderer, FONT_UI, 15, yPos, name.c_str(), sw - 90, !tc);
      } else {
        drawClippedText(renderer, FONT_UI, 15, yPos, name.c_str(), sw - 90, tc);
      }

      if (active) {
        drawRightText(renderer, FONT_SMALL, sw - 10, yPos + 2, tr(StrId::TAG_ACTIVE), sel ? !tc : tc);
      } else if (!active && i == getLastUsedKeyboardIndex() && currentAddr.empty()) {
        drawRightText(renderer, FONT_SMALL, sw - 10, yPos + 2, tr(StrId::TAG_LAST), sel ? !tc : tc);
      }
    }
  }

  constexpr int bm = 62;
  if (sh > bm + 30) {
    clippedLine(renderer, 10, sh - bm, sw - 10, sh - bm, tc);
    if (pairedForgetPending && count > 0) {
      std::string addr, name; uint8_t addrType;
      getPairedKeyboard(pairedKeyboardSelection, addr, name, addrType);
      char prompt[64];
      snprintf(prompt, sizeof(prompt), tr(StrId::FORGET_FMT), name.c_str());
      drawClippedText(renderer, FONT_SMALL, 10, sh - bm + 10, prompt, sw - 20, tc, EpdFontFamily::BOLD);
      drawClippedText(renderer, FONT_SMALL, 10, sh - bm + 32, tr(StrId::HINT_YES_NO), 0, tc);
    } else {
      drawClippedText(renderer, FONT_SMALL, 10, sh - bm + 10, tr(StrId::HINT_PAIRED_1), 0, tc);
      drawClippedText(renderer, FONT_SMALL, 10, sh - bm + 32, tr(StrId::HINT_PAIRED_2), 0, tc);
    }
  }

  renderer.beginRefresh(HalDisplay::FAST_REFRESH);
}

// Helper: draw signal strength indicator (1-4 bars)
static void drawSignalBars(GfxRenderer& r, int x, int y, int rssi, bool color) {
  // RSSI to bars: > -50 = 4, > -65 = 3, > -75 = 2, else 1
  int bars = (rssi > -50) ? 4 : (rssi > -65) ? 3 : (rssi > -75) ? 2 : 1;
  for (int i = 0; i < 4; i++) {
    int bh = 4 + i * 3;  // bar heights: 4, 7, 10, 13
    int by = y + 13 - bh;
    if (i < bars) {
      clippedFillRect(r, x + i * 5, by, 3, bh, color);
    } else {
      clippedFillRect(r, x + i * 5, by + bh - 2, 3, 2, color);
    }
  }
}

void drawSyncScreen(GfxRenderer& renderer, HalGPIO& gpio) {
  renderer.clearScreen();
  int sw = renderer.getScreenWidth();
  int sh = renderer.getScreenHeight();
  bool tc = !darkMode;

  if (darkMode) clippedFillRect(renderer, 0, 0, sw, sh, true);

  // Header
  drawClippedText(renderer, FONT_SMALL, 10, 5, tr(StrId::TITLE_SYNC), 0, tc, EpdFontFamily::BOLD);
  drawBattery(renderer, gpio);
  clippedLine(renderer, 5, 32, sw - 5, 32, tc);

  SyncState state = getSyncState();

  switch (state) {
    case SyncState::SCANNING: {
      drawClippedText(renderer, FONT_UI, 20, 80, tr(StrId::SCANNING_NETWORKS), sw - 40, tc);
      break;
    }

    case SyncState::NETWORK_LIST: {
      int nc = getNetworkCount();
      int sel = getSelectedNetwork();

      if (nc == 0) {
        const char* st = getSyncStatusText();
        drawClippedText(renderer, FONT_UI, 20, 60, st[0] ? st : tr(StrId::NO_NETWORKS), sw - 40, tc);
        drawClippedText(renderer, FONT_SMALL, 20, 90, tr(StrId::HINT_RESCAN), 0, tc);
      } else {
        drawClippedText(renderer, FONT_SMALL, 10, 38, tr(StrId::SELECT_NETWORK), 0, tc);

        int lineH = 28;
        int listTop = 56;
        int footerH = 28;
        int maxVisible = (sh - listTop - footerH) / lineH;
        int startIdx = 0;
        if (nc > maxVisible && sel >= maxVisible) {
          startIdx = sel - maxVisible + 1;
        }

        for (int i = startIdx; i < nc && (i - startIdx) < maxVisible; i++) {
          int yPos = listTop + (i - startIdx) * lineH;
          bool isSel = (i == sel);

          // Build display string: signal indicator + lock + saved + SSID
          char label[48];
          snprintf(label, sizeof(label), "%s%s%s",
                   isNetworkEncrypted(i) ? "* " : "  ",
                   isNetworkSaved(i) ? "+ " : "",
                   getNetworkSSID(i));

          if (isSel) {
            clippedFillRect(renderer, 5, yPos - 3, sw - 10, lineH - 2, tc);
            drawClippedText(renderer, FONT_UI, 15, yPos, label, sw - 50, !tc);
            drawSignalBars(renderer, sw - 30, yPos, getNetworkRSSI(i), !tc);
          } else {
            drawClippedText(renderer, FONT_UI, 15, yPos, label, sw - 50, tc);
            drawSignalBars(renderer, sw - 30, yPos, getNetworkRSSI(i), tc);
          }
        }
      }

      // Footer
      constexpr int bm = 28;
      clippedLine(renderer, 10, sh - bm - 2, sw - 10, sh - bm - 2, tc);
      drawClippedText(renderer, FONT_SMALL, 10, sh - bm + 4,
                      tr(StrId::HINT_NETLIST), 0, tc);
      break;
    }

    case SyncState::PASSWORD_ENTRY: {
      int sel = getSelectedNetwork();
      char heading[64];
      snprintf(heading, sizeof(heading), tr(StrId::PASSWORD_FOR_FMT), getNetworkSSID(sel));
      drawClippedText(renderer, FONT_SMALL, 20, 42, heading, sw - 40, tc);

      // Password field box
      renderer.drawRect(15, 62, sw - 30, 30, tc);

      // Show dots for password characters (privacy)
      int pLen = getPasswordLen();
      char dots[64];
      for (int i = 0; i < pLen; i++) dots[i] = '*';
      dots[pLen] = '\0';
      drawClippedText(renderer, FONT_UI, 20, 66, dots, sw - 50, tc);

      // Cursor
      int cursorX = 20 + renderer.getTextAdvanceX(FONT_UI, dots);
      int cursorW = renderer.getSpaceWidth(FONT_UI);
      if (cursorW < 2) cursorW = 8;
      if (cursorX + cursorW < sw)
        renderer.fillRect(cursorX, 66, cursorW, 20, tc);

      drawClippedText(renderer, FONT_SMALL, 20, 104, tr(StrId::PASSWORD_QWERTY), sw - 40, tc);
      drawClippedText(renderer, FONT_SMALL, 20, 122, tr(StrId::HINT_CONNECT), 0, tc);
      break;
    }

    case SyncState::CONNECTING: {
      const char* st = getSyncStatusText();
      drawClippedText(renderer, FONT_UI, 20, 80, st, sw - 40, tc);
      drawClippedText(renderer, FONT_SMALL, 20, 110, tr(StrId::HINT_CANCEL), 0, tc);
      break;
    }

    case SyncState::SYNCING: {
      const char* ip = getSyncStatusText();
      drawClippedText(renderer, FONT_SMALL, 20, 42, ip, sw - 40, tc, EpdFontFamily::BOLD);

      int sent    = getSyncFilesSent();
      bool pcConn = isPcConnected();

      // Build the stage list. Static so string pointers remain valid after this scope.
      static char fileStageText[MAX_FILES][32];
      static struct { const char* text; bool done; } stages[MAX_FILES + 3];
      int numStages = 0;

      auto push = [&](const char* text, bool done) {
        if (numStages < MAX_FILES + 3) stages[numStages++] = { text, done };
      };

      push(tr(StrId::STAGE_WIFI), true);
      push(tr(StrId::STAGE_PC),   pcConn);

      if (pcConn) {
        for (int i = 0; i < sent && i < MAX_FILES; i++) {
          snprintf(fileStageText[i], sizeof(fileStageText[i]), tr(StrId::STAGE_FILE_FMT), i + 1);
          push(fileStageText[i], true);
        }
        push(tr(StrId::STAGE_SYNC_DONE), false);
      }

      // Display with auto-scroll: keep last [x] + next [-] in view
      constexpr int lineH   = 22;
      constexpr int listTop = 62;
      constexpr int footerH = 32;
      int maxVisible = (sh - listTop - footerH) / lineH;

      int lastDone = 0;
      for (int i = 0; i < numStages; i++) {
        if (stages[i].done) lastDone = i;
      }
      int startIdx = 0;
      if (numStages > maxVisible) {
        startIdx = lastDone - maxVisible + 2;
        if (startIdx < 0) startIdx = 0;
        if (startIdx + maxVisible > numStages) startIdx = numStages - maxVisible;
      }

      for (int i = startIdx; i < numStages && (i - startIdx) < maxVisible; i++) {
        int yPos = listTop + (i - startIdx) * lineH;
        char line[64];
        snprintf(line, sizeof(line), "%s %s",
                 stages[i].done ? "[x]" : "[-]", stages[i].text);
        drawClippedText(renderer, FONT_SMALL, 15, yPos, line, sw - 25, tc);
      }

      // Footer
      constexpr int bm = 28;
      clippedLine(renderer, 10, sh - bm - 2, sw - 10, sh - bm - 2, tc);
      char countStr[48];
      snprintf(countStr, sizeof(countStr), tr(StrId::SENT_CANCEL_FMT), sent);
      drawClippedText(renderer, FONT_SMALL, 10, sh - bm + 4, countStr, sw - 20, tc);
      break;
    }

    case SyncState::DONE: {
      const char* summary = getSyncStatusText();
      drawClippedText(renderer, FONT_SMALL, 20, 50, tr(StrId::SYNC_COMPLETE), 0, tc, EpdFontFamily::BOLD);
      drawClippedText(renderer, FONT_UI, 20, 85, summary, sw - 40, tc);
      drawClippedText(renderer, FONT_SMALL, 20, 125, tr(StrId::RETURNING), 0, tc);
      break;
    }

    case SyncState::CONNECT_FAILED: {
      drawClippedText(renderer, FONT_UI, 20, 80, tr(StrId::CONN_FAILED), sw - 40, tc);
      drawClippedText(renderer, FONT_SMALL, 20, 110, tr(StrId::HINT_RETRY), 0, tc);
      break;
    }

    case SyncState::SAVE_PROMPT: {
      const char* ip = getSyncStatusText();
      drawClippedText(renderer, FONT_SMALL, 20, 50, tr(StrId::CONNECTED_BANG), 0, tc, EpdFontFamily::BOLD);
      drawClippedText(renderer, FONT_UI, 20, 80, ip, sw - 40, tc);
      drawClippedText(renderer, FONT_SMALL, 20, 120, tr(StrId::SAVE_PASSWORD), 0, tc, EpdFontFamily::BOLD);
      drawClippedText(renderer, FONT_SMALL, 20, 145, tr(StrId::HINT_YES_NO_ARROWS), 0, tc);
      break;
    }

    case SyncState::FORGET_PROMPT: {
      drawClippedText(renderer, FONT_UI, 20, 80, tr(StrId::SAVED_PW_FAILED), sw - 40, tc);
      drawClippedText(renderer, FONT_SMALL, 20, 120, tr(StrId::FORGET_PASSWORD), 0, tc, EpdFontFamily::BOLD);
      drawClippedText(renderer, FONT_SMALL, 20, 145, tr(StrId::HINT_YES_NO_ARROWS), 0, tc);
      break;
    }
  }

  renderer.beginRefresh(HalDisplay::FAST_REFRESH);
}
