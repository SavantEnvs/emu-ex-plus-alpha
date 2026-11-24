#!/usr/bin/env bash
#
# mayhem/test.sh — run the known-answer self-tests built by mayhem/build.sh.
# Upstream ships no test suite; these check behavior of the same core code the harnesses
# fuzz: libgambatte ROM loading, CPU execution into cartridge RAM, frame output and the
# save-state round trip (GBC.emu), the IPS/UPS patchers (GBA.emu), and Lynx cartridge
# loading (format detection, MD5, rotation) and emulation (Lynx.emu). Each check prints
# PASS/FAIL <name>; a no-op binary prints nothing and fails the expected-count check.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

passed=0; failed=0
# run_suite <binary> <expected number of checks>
run_suite() {
  local bin="$SRC/mayhem/out/$1" want="$2" out p f
  if [ ! -x "$bin" ]; then echo "missing $bin — run mayhem/build.sh" >&2; failed=$((failed + want)); return; fi
  out="$("$bin" 2>&1)"; local rc=$?
  echo "$out"
  p=$(grep -c '^PASS ' <<<"$out"); f=$(grep -c '^FAIL ' <<<"$out")
  # Missing checks (binary stopped early or did nothing) count as failures.
  if [ $((p + f)) -lt "$want" ]; then f=$((want - p)); fi
  if [ "$rc" -ne 0 ] && [ "$f" -eq 0 ]; then f=1; fi
  passed=$((passed + p)); failed=$((failed + f))
}
run_suite gbc_selftest 19
run_suite gba_patch_selftest 13
run_suite lynx_selftest 11
run_suite snes9x_selftest 14
emit_ctrf "mayhem-selftest" "$passed" "$failed" 0
