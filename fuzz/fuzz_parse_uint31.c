/*
 * Copyright (C) 2026 - 2026 ENCRYPTED SUPPORT LLC <adrelanos@whonix.org>
 * See the file COPYING for copying conditions.
 *
 * AI-Assisted
 *
 * libFuzzer harness for kloak's parse_uint31_arg() string-to-int31
 * parser. parse_uint31_arg() is called from parse_cli_args() to
 * handle --max-delay and --startup-delay; an out-of-spec value
 * would land in security-relevant timing logic, so it is worth a
 * targeted fuzz pass.
 *
 * The parsers in kloak.c were specifically authored to return
 * false on bad input rather than exit() (see the docstring at
 * kloak.c:892), so they are directly fuzz-target safe.
 *
 * Pulling kloak.c into the harness via #include keeps the
 * file-scope `static` qualifier on the target intact (no
 * exported-symbol surface change) while letting the harness call
 * the parser directly. Dead code in kloak.c (Wayland callbacks,
 * libinput dispatch) is linked in but never executed during
 * fuzzing.
 */

#ifndef KLOAK_FUZZING
#define KLOAK_FUZZING
#endif
#include "../src/kloak.c"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * Bases parse_uint31_arg supports in practice are 10 (CLI integer
 * args) and 16 (cursor color is uint32 not int31, so 16 is more
 * relevant to the sibling harness, but feeding it here exercises
 * the strtoul path for negative-prefix edge cases).
 */
static const int kBases[] = {10, 16, 8, 2};

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

  int32_t out = 0;
  (void)parse_uint31_arg(buf, base, &out);

  free(buf);
  return 0;
}
