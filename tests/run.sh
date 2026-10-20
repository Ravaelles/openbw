#!/usr/bin/env bash
# Run the engine behaviour tests against real maps.
#
# These link bwgame.h directly (no BWAPI, no bot, no running match), so they are
# a fast pre-commit check for the UMS features.
#
# Usage:
#   UMS_TEST_DATA_DIR=/path/with/mpq tests/run.sh MAP [MAP...]
#   UMS_TEST_DATA_DIR=... tests/run.sh --default-maps     # the two known maps
#
# Exit codes: 0 all maps passed, 1 a check failed, 2 setup problem.
set -uo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/.." && pwd)"

: "${UMS_TEST_DATA_DIR:?set UMS_TEST_DATA_DIR to the directory containing StarDat.mpq/BrooDat.mpq/Patch_rt.mpq}"

maps=("$@")
if [ "${1:-}" = "--default-maps" ]; then
  maps=(
    "/sc-ai/starcraft/maps/ums/rav/protoss/1Drag_v_1Zeal.scm"
    "/sc-ai/StardustDevEnvironment/build/test/maps/ums/rav/terran/M&M_v_ZealDrag.scx"
  )
fi
if [ ${#maps[@]} -eq 0 ]; then
  echo "usage: UMS_TEST_DATA_DIR=... $0 MAP [MAP...]" >&2
  exit 2
fi

bin="$(mktemp -t engine_behaviour_test.XXXXXX)"
trap 'rm -f "$bin"' EXIT

echo "building $bin from $root"
if ! g++ -std=c++17 -O2 -I"$root" "$here/engine_behaviour_test.cpp" -o "$bin" -lz -lbz2; then
  echo "FAIL: build failed" >&2
  exit 2
fi

failed=0
for m in "${maps[@]}"; do
  [ -f "$m" ] || { echo "FAIL: no such map: $m" >&2; failed=1; continue; }
  echo
  echo "=== $(basename "$m")"
  if ! UMS_TEST_DATA_DIR="$UMS_TEST_DATA_DIR" "$bin" "$m"; then
    failed=1
  fi
done

if [ "$failed" -ne 0 ]; then
  echo
  echo "FAILED"
  exit 1
fi
echo
echo "ALL PASSED"
exit 0
