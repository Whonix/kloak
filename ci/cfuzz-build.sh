#!/bin/bash

## Copyright (C) 2026 - 2026 ENCRYPTED SUPPORT LLC <adrelanos@whonix.org>
## See the file COPYING for copying conditions.

## AI-Assisted

## Build script for kloak libFuzzer harnesses.
##
## Two entry contexts:
##
## 1. Invoked by .clusterfuzzlite/build.sh inside the OSS-Fuzz
##    base-builder container. $SRC / $OUT / $CC / $CFLAGS /
##    $LIB_FUZZING_ENGINE are pre-set by the container. Build deps
##    are pre-installed by the Dockerfile.
##
## 2. Invoked by ci/cfuzz-run.sh on the host (no Docker, no
##    sanitizer-aware $CC). Picks sensible defaults: clang +
##    -fsanitize=fuzzer,address,undefined. Useful for local
##    developer iteration without spinning up Docker.

set -o errexit
set -o nounset
set -o pipefail
set -o errtrace
shopt -s inherit_errexit
shopt -s shift_verbose

## Resolve repo root regardless of caller's cwd.
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
cd -- "${REPO_ROOT}"

## --- Mode detection -------------------------------------------------

## ClusterFuzzLite path: $OUT is set, $CC / $CFLAGS /
## $LIB_FUZZING_ENGINE pre-baked by base-builder.
## Local-dev path: nothing pre-set, fall back to clang + libFuzzer.
if [ -z "${OUT:-}" ]; then
  OUT="${REPO_ROOT}/out"
  mkdir -p -- "${OUT}"
fi
if [ -z "${CC:-}" ]; then
  CC="clang"
fi
if [ -z "${CFLAGS:-}" ]; then
  ## TODO: Right now we rely on '-ftrapv' to cause any signed integer overflow
  ## to crash the kloak process. kloak even goes so far as to use signed
  ## arithmetic even in places where unsigned arithmetic would be more
  ## intuitive, just so that it can get overflow protection "for free". We
  ## can't use '-ftrapv' here though, as that would crash the process being
  ## fuzzed. Without it, the fuzzer is finding many false positives. Is there
  ## any easy way to tell the fuzzer to treat signed integer overflow as an
  ## "immediately abort the current test run and consider it successful"
  ## condition without crashing the process entirely, or do we need to do the
  ## (probably very painful) work of explicitly checking for integer overflow
  ## everywhere it can happen?
  ##
  ## Possibly better idea than checking for overflow before calculations;
  ## code as if '-fwrapv' is being used? That is, do arithmetic that could
  ## overflow, then check for overflow after the fact. We can then use
  ## '-ftrapv' in production (crash on overflow), and '-fwrapv' for the
  ## fuzzer. This is still painful, but not as painful as the alternative.
  CFLAGS="-O1 -g -fno-omit-frame-pointer -fsanitize=fuzzer-no-link,address,undefined"
fi
if [ -z "${LIB_FUZZING_ENGINE:-}" ]; then
  LIB_FUZZING_ENGINE="-fsanitize=fuzzer"
fi

## --- Wayland protocol codegen ---------------------------------------
##
## kloak.c #include's the generated protocol headers, so we need
## wayland-scanner to produce them before compiling any harness.
## Mirror the Makefile's targets.

PROTO_SRC=()
for proto_pair in \
  "xdg-shell.xml:src/xdg-shell-protocol" \
  "xdg-output-unstable-v1.xml:src/xdg-output-protocol" \
  "wlr-layer-shell-unstable-v1.xml:src/wlr-layer-shell" \
  "wlr-virtual-pointer-unstable-v1.xml:src/wlr-virtual-pointer" \
  "virtual-keyboard-unstable-v1.xml:src/virtual-keyboard" ; do
  xml="${proto_pair%%:*}"
  base="${proto_pair#*:}"
  wayland-scanner client-header < "protocol/${xml}" > "${base}.h"
  wayland-scanner private-code  < "protocol/${xml}" > "${base}.c"
  PROTO_SRC+=("${base}.c")
done

## --- Harness compile loop -------------------------------------------
##
## Each harness #include's src/kloak.c with -DKLOAK_FUZZING so
## main() is excluded; the harness file itself supplies
## LLVMFuzzerTestOneInput. We compile the protocol .c files into
## the same binary because kloak.c's body references their
## interface symbols even when those code paths are dead.
##
## pkg-config provides cflags/libs for libinput, libevdev,
## wayland-client, xkbcommon - the same set kloak's main Makefile
## consumes.

PKG_LIBS="libinput libevdev wayland-client xkbcommon"
PKG_CFLAGS_VALUE="$(pkg-config --cflags ${PKG_LIBS})"
PKG_LIBS_VALUE="$(pkg-config --libs ${PKG_LIBS})"

for harness in fuzz/fuzz_*.c; do
  name="$(basename -- "${harness}" .c)"
  printf 'building %s\n' "${name}"

  # shellcheck disable=SC2086
  ${CC} ${CFLAGS} \
    -DKLOAK_FUZZING \
    ${PKG_CFLAGS_VALUE} \
    "${harness}" \
    "${PROTO_SRC[@]}" \
    -o "${OUT}/${name}" \
    ${LIB_FUZZING_ENGINE} \
    -lm -lrt \
    ${PKG_LIBS_VALUE}

  printf 'compiled %s -> %s\n' "${name}" "${OUT}/${name}"
done
