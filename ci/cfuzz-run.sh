#!/bin/bash

## Copyright (C) 2026 - 2026 ENCRYPTED SUPPORT LLC <adrelanos@whonix.org>
## See the file COPYING for copying conditions.

## AI-Assisted

## Local-runner script for kloak libFuzzer harnesses. Builds via
## ci/cfuzz-build.sh (which auto-falls-back to clang + libFuzzer
## when not invoked from inside the OSS-Fuzz container) and runs
## each harness with a configurable time budget.
##
## This script is for developer iteration. The ClusterFuzzLite CI
## path uses google/clusterfuzzlite/actions/{build_fuzzers,run_fuzzers}
## directly and does NOT call this script.
##
## Env vars:
##   FUZZ_SECONDS - per-harness time budget. Default 30.
##   FUZZ_MAX_LEN - libFuzzer -max_len. Default 4096.

set -o errexit
set -o nounset
set -o pipefail
set -o errtrace

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
cd -- "${REPO_ROOT}"

FUZZ_SECONDS="${FUZZ_SECONDS:-30}"
FUZZ_MAX_LEN="${FUZZ_MAX_LEN:-4096}"

## Build (no-op-equivalent if already built; the build script
## always regenerates protocol headers + relinks).
ci/cfuzz-build.sh

failed=0
for binary in out/fuzz_*; do
  if [ ! -x "${binary}" ]; then
    continue
  fi
  name="$(basename -- "${binary}")"
  printf '\n=== %s (%ss budget) ===\n' "${name}" "${FUZZ_SECONDS}"
  if ! "${binary}" \
       -max_total_time="${FUZZ_SECONDS}" \
       -max_len="${FUZZ_MAX_LEN}" \
       -print_final_stats=1 ; then
    printf '%s: FAILED\n' "${name}" >&2
    failed=$((failed + 1))
  fi
done

if [ "${failed}" -gt 0 ]; then
  printf '\n%d harness(es) failed.\n' "${failed}" >&2
  exit 1
fi

printf '\nall harnesses passed.\n'
