# kloak: outstanding TODOs

## Fuzzing

Currently unwired. Scorecard reports `FuzzingID = 0/10` and
that's accurate.

### Why not done in the same pass as security-misc Hypothesis

The realistic kloak fuzz target is the input event loop +
Wayland callback surface. Both are deeply integrated with
external state (real `/dev/input/event*` device, real Wayland
server) and don't have clean entry points that take a byte
buffer + length the way libFuzzer wants.

Two tractable single-file targets:

- `parse_uint32_arg` (kloak.c:917): strtoul wrapper. **Calls
  `exit(1)` on parse error**, which kills libFuzzer, so a
  refactor is required first - extract a value-only variant
  (`parse_uint32_arg_value()` returning int) plus the existing
  exit-on-error wrapper. Bug surface is small (mostly strtoul
  edge cases) but the harness is genuinely useful.
- `lookup_keycode` (kloak.c:983): pure strcmp loop. Too simple
  for fuzzing to find anything.

Plan when this is picked up:

1. Refactor `parse_uint32_arg` into `_value` + wrapper, no
   behavior change for runtime callers.
2. Add `fuzz/fuzz_parse_uint32_arg.c` libFuzzer harness.
3. Add ClusterFuzzLite scaffolding mirroring helper-scripts:
   `.clusterfuzzlite/{Dockerfile,build.sh,project.yaml}` +
   `.github/workflows/fuzz.yml`. Use the C-toolchain build
   variant of the OSS-Fuzz base image (the helper-scripts
   setup is Python/Atheris specific).
4. Run for a few cycles before adding more targets.

The deeper Wayland-callback surface stays manual review only
until someone writes a state-machine fuzzer (out of scope for
the org's current fuzzing budget).
