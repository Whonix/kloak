#!/bin/bash

## Copyright (C) 2026 - 2026 ENCRYPTED SUPPORT LLC <adrelanos@whonix.org>
## See the file COPYING for copying conditions.

## AI-Assisted

## ClusterFuzzLite C/C++ build script. Invoked inside the
## OSS-Fuzz base-builder container by the ClusterFuzzLite tooling.
##
## Standard OSS-Fuzz contract:
##   - $SRC                - source root (we COPY the repo here in
##                            the Dockerfile)
##   - $OUT                - output directory; harnesses go here
##   - $CC / $CXX          - sanitizer-aware compilers
##   - $CFLAGS / $CXXFLAGS - sanitizer-aware flags
##   - $LIB_FUZZING_ENGINE - path/flag for the fuzzing-engine archive
##                            (libFuzzer.a or the honggfuzz equivalent)
##
## The body of the build (wayland-scanner codegen + harness
## compile loop) lives in ci/cfuzz-build.sh to keep this entry
## point a thin shim and follow R-100 ("no inline scripts
## beyond invocation").

set -o errexit
set -o nounset
set -o pipefail
set -o errtrace

## NOTE: no CI-guard here. This script is invoked by ClusterFuzzLite
## inside the OSS-Fuzz base-builder container; it does not see the
## GitHub Actions CI=true env var. The trust boundary is the
## container itself, not this script.

cd -- "$SRC/kloak"

exec ci/cfuzz-build.sh
