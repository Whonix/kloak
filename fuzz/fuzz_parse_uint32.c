/*
 * Copyright (C) 2026 - 2026 ENCRYPTED SUPPORT LLC <adrelanos@whonix.org>
 * See the file COPYING for copying conditions.
 *
 * AI-Assisted
 *
 * libFuzzer harness for kloak's parse_uint32_arg() string-to-uint32
 * parser. Called from parse_cli_args() to handle --cursor-color
 * (base 16). Sibling to fuzz_parse_uint31; same #include +
 * KLOAK_FUZZING approach.
 */

#ifndef KLOAK_FUZZING
#define KLOAK_FUZZING
#endif
#include "../src/kloak.c"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const int kBases[] = {16, 10, 8, 2};

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  if (size < 1U) {
    return 0;
  }
  const int base = kBases[data[0] & 0x3];
  const size_t slen = size - 1U;

  char *buf = malloc(slen + 1U);
  if (buf == NULL) {
    return 0;
  }
  memcpy(buf, data + 1, slen);
  buf[slen] = '\0';

  uint32_t out = 0;
  (void)parse_uint32_arg(buf, base, &out);

  free(buf);
  return 0;
}
