#!/usr/bin/env bash
# Run every engine test suite and print a summary: how many suites passed, how
# many individual checks passed, and which ones failed.
#
# Why this exists: tests/run.sh runs the suites for given maps, but reading its
# output to answer "did everything pass?" means scrolling through hundreds of
# "ok" lines. This wrapper builds each suite once, runs it against every map,
# counts the results, and prints one block per failing check so the reason is
# visible without opening the logs.
#
# Usage:
#   ./_run-tests.sh                      # default maps + all suites
#   ./_run-tests.sh --map a.scm b.scm    # explicit maps
#   ./_run-tests.sh --quick              # one map only (fast smoke)
#   ./_run-tests.sh --list               # list suites and maps, run nothing
#
# Environment:
#   UMS_TEST_DATA_DIR   directory with StarDat.mpq / BrooDat.mpq / Patch_rt.mpq
#                       (default: /tmp/scdata, the harness copy)
#
# Exit codes: 0 everything passed, 1 at least one check failed, 2 setup problem.
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TESTS="$ROOT/tests"
DATA_DIR="${UMS_TEST_DATA_DIR:-/tmp/scdata}"
BUILD_DIR="${TESTS_BUILD_DIR:-/tmp/openbw-tests}"

DEFAULT_MAPS=(
  "/sc-ai/starcraft/maps/ums/rav/protoss/1Drag_v_1Zeal.scm"
  "/sc-ai/StardustDevEnvironment/build/test/maps/ums/rav/terran/M&M_v_ZealDrag.scx"
)

maps=("${DEFAULT_MAPS[@]}")
mode=full
while [ $# -gt 0 ]; do
  case "$1" in
    --map) shift; maps=(); while [ $# -gt 0 ] && [ "${1#--}" = "$1" ]; do maps+=("$1"); shift; done ;;
    --quick) mode=quick; shift ;;
    --list) mode=list; shift ;;
    -h|--help) sed -n '2,24p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done
[ "$mode" = quick ] && maps=("${DEFAULT_MAPS[0]}")

suites=()
while IFS= read -r f; do suites+=("$f"); done < <(ls "$TESTS"/*_test.cpp 2>/dev/null | sort)
[ ${#suites[@]} -gt 0 ] || { echo "no test sources in $TESTS" >&2; exit 2; }
command -v g++ >/dev/null 2>&1 || { echo "g++ not found" >&2; exit 2; }

if [ "$mode" = list ]; then
  echo "suites:"; for s in "${suites[@]}"; do echo "  $(basename "$s" .cpp)"; done
  echo "maps:"; for m in "${maps[@]}"; do echo "  $m"; done
  exit 0
fi

mkdir -p "$BUILD_DIR"
[ -d "$DATA_DIR" ] || { echo "UMS_TEST_DATA_DIR=$DATA_DIR does not exist" >&2; exit 2; }

# ---- build -----------------------------------------------------------------
echo "building ${#suites[@]} suites..."
build_failed=0
for src in "${suites[@]}"; do
  name="$(basename "$src" .cpp)"
  out="$BUILD_DIR/$name"
  if ! g++ -std=c++17 -O2 -I"$ROOT" "$src" -o "$out" -lz -lbz2 2>"$BUILD_DIR/$name.build.log"; then
    echo "  BUILD FAILED: $name (see $BUILD_DIR/$name.build.log)"
    head -5 "$BUILD_DIR/$name.build.log" | sed 's/^/    /'
    build_failed=$((build_failed + 1))
  fi
done
[ "$build_failed" -eq 0 ] || { echo; echo "FAILED: $build_failed suite(s) did not build"; exit 1; }

# ---- run -------------------------------------------------------------------
# Counters. "checks" are the individual ok/FAIL lines each suite prints.
total_ok=0; total_fail=0; suites_pass=0; suites_fail=0
fail_report=""
printf '\n%-34s %-26s %6s %6s\n' "SUITE" "MAP" "PASS" "FAIL"
printf '%.0s-' $(seq 1 78); echo

for src in "${suites[@]}"; do
  name="$(basename "$src" .cpp)"
  bin="$BUILD_DIR/$name"
  for map in "${maps[@]}"; do
    if [ ! -f "$map" ]; then
      printf '%-34s %-26s %6s %6s\n' "$name" "MISSING" "-" "-"
      suites_fail=$((suites_fail + 1))
      fail_report+="MISSING MAP: $map (suite $name)"$'\n'
      continue
    fi
    log="$BUILD_DIR/$name.$(basename "$map").log"
    UMS_TEST_DATA_DIR="$DATA_DIR" "$bin" "$map" >"$log" 2>&1
    rc=$?
    ok=$(grep -c '^ok ' "$log" 2>/dev/null || true)
    bad=$(grep -c '^FAIL' "$log" 2>/dev/null || true)
    ok=${ok:-0}; bad=${bad:-0}
    total_ok=$((total_ok + ok)); total_fail=$((total_fail + bad))
    if [ "$rc" -eq 0 ] && [ "$bad" -eq 0 ]; then
      suites_pass=$((suites_pass + 1)); status=""
    else
      suites_fail=$((suites_fail + 1)); status="  <-- failed"
      fail_report+="--- $name on $(basename "$map") (exit $rc)"$'\n'
      fail_report+="$(grep '^FAIL' "$log" | sed 's/^/    /')"$'\n'
      # a crash produces no FAIL line: surface the tail so it is not silent
      [ "$bad" -eq 0 ] && fail_report+="$(tail -5 "$log" | sed 's/^/    /')"$'\n'
    fi
    printf '%-34s %-26s %6s %6s%s\n' "$name" "$(basename "$map")" "$ok" "$bad" "$status"
  done
done

# ---- summary ---------------------------------------------------------------
total_runs=$((suites_pass + suites_fail))
checks=$((total_ok + total_fail))
echo
echo "=============================================================="
echo "suite runs : $total_runs   passed: $suites_pass   failed: $suites_fail"
echo "checks     : $checks   passed: $total_ok   failed: $total_fail"
if [ "$total_fail" -eq 0 ] && [ "$suites_fail" -eq 0 ]; then
  echo "RESULT: ALL PASSED"
  echo "=============================================================="
  exit 0
fi
echo "RESULT: FAILURES"
echo
echo "Failing checks:"
printf '%s' "$fail_report"
echo "Logs: $BUILD_DIR"
echo "=============================================================="
exit 1
