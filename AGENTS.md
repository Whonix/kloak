# AGENTS.md - Coding Guidelines for AI Agents

Guidelines learned from upstream review of security hardening patches
(Whonix/kloak#11, reviewed by @ArrayBolt3, integrated in ArrayBolt3@f2dfa9e).

## C Coding Style

### Assertions vs. Runtime Error Handling

- **Rely on assertions.** Unless there is a very good reason to print a detailed
  error message, all data validation and bounds checking should be done with
  `assert()`, not verbose `fprintf()`+`exit()` error handling. `assert()` is
  just as reliable as more complicated checks, and is easier to read. The
  build system enforces that `NDEBUG` is never defined (see the check in
  `main()`), so asserts are always active.
- Reserve `fprintf(stderr, "FATAL ERROR: ...")`+`exit(1)` for untrusted external
  input (user CLI arguments, file I/O errors, system call failures) and
  situations where detailed debugging output is useful.

### Floating-Point Validation

- Prefer `isfinite()` (from `<math.h>`) over two separate
  `fpclassify() != FP_NAN` / `fpclassify() != FP_INFINITE` checks. It is
  functionally equivalent and more concise.

### Integer Parsing

- When using `strtoul()`, always set `errno = 0` before the call and check
  `errno == ERANGE` after, in addition to existing range checks.
- Watch for copy-paste errors in goto labels. Each function should have its own
  uniquely named error label (e.g. `parse_uint32_arg_error` not
  `parse_uint31_arg_error`).

### Integer Overflow

- Use signed arithmetic wherever possible. kloak compiles with `-ftrapv`, so
  signed overflow traps (crashes) rather than silently wrapping. Unsigned
  overflow is well-defined in C and does NOT trap, making it more dangerous.
- Before multiplying sizes, assert the operand is within bounds to prevent
  overflow: `assert(val <= INT32_MAX / MULTIPLIER)`.

### Array Bounds

- Always assert both lower AND upper bounds before indexing into fixed-size
  arrays (e.g. `assert(idx >= 0 && idx < MAX_SCREEN_COUNT)`).

### inotify-Specific

- `ie->len` is unsigned, so checking `struct_len < 0` is unnecessary.
- Do check `ie->len < SSIZE_MAX - sizeof(struct inotify_event)` to guard
  against wraparound when computing total struct length.
