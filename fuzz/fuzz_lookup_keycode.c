/*
 * Copyright (C) 2026 - 2026 ENCRYPTED SUPPORT LLC <adrelanos@whonix.org>
 * See the file COPYING for copying conditions.
 *
 * AI-Assisted
 *
 * libFuzzer harness for kloak's lookup_keycode(). Called from
 * parse_esc_key_str() to map a CLI-supplied key name (sub-token
 * of --esc-keys) onto a uint32 keycode. The implementation is a
 * strcmp() loop over a static table; the harness exists to assert
 * the contract under ASan/UBSan (no OOB read, no UB on weird
 * inputs including embedded high bytes and short inputs) rather
 * than to find a complex parsing bug.
 */

#ifndef KLOAK_FUZZING
#define KLOAK_FUZZING
#endif
#include "../src/kloak.c"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  char *buf = malloc(size + 1U);
  if (buf == NULL) {
    return 0;
  }
  memcpy(buf, data, size);
  buf[size] = '\0';

  (void)lookup_keycode(buf);

  free(buf);
  return 0;
}
