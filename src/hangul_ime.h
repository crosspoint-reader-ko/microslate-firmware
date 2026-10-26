#pragma once

#include <cstdint>

// Korean 2-set (dubeolsik) input composer.
//
// The composer holds at most one syllable in progress (the "preedit"). Each
// call reports what to do with the text field:
//   - remove the old preedit (if `hadPreedit`),
//   - append `commit` (a finished syllable, may be empty),
//   - append the new `preedit` (may be empty).
// Every string is UTF-8 and at most one code point, so 4 bytes fit.
//
// No Arduino dependency, so it can be unit-tested on the host.

struct HangulEdit {
  bool hadPreedit;  // a preedit char was in the field before this call
  char commit[4];
  char preedit[4];
};

// Compatibility jamo (U+3131..U+3163) for a 2-set key, or 0 if the key has none.
// `hidKey` is a USB HID usage code for a letter key; Shift picks the tense
// consonants (ㄲㄸㅃㅆㅉ) and ㅒ/ㅖ.
uint32_t hangulJamoForKey(uint8_t hidKey, bool shift);

// Feed one compatibility jamo.
HangulEdit hangulInput(uint32_t jamo);

// Undo the last jamo of the syllable in progress. Returns false (and does
// nothing) when nothing is being composed, so the caller deletes normally.
bool hangulBackspace(HangulEdit* out);

// Finish composing. The preedit is already in the field, so this only drops
// the state; the next key starts a new syllable.
void hangulCommit();

bool hangulIsComposing();
