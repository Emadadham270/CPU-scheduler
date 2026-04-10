#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT_DIR"

PASS_COUNT=0
TOTAL_COUNT=3

run_step() {
  local name="$1"
  shift
  echo "[RUN] $name"
  "$@"
  echo "[PASS] $name"
  PASS_COUNT=$((PASS_COUNT + 1))
  echo
}

echo "== Building required runtime targets =="
make clk process

echo "== Optional scheduler build check =="
if make scheduler >/dev/null 2>&1; then
  echo "[PASS] scheduler target builds"
else
  echo "[WARN] scheduler target failed to build (not required for current test set)"
fi

echo "== Compiling test executables =="
mkdir -p outFiles

gcc -Wall -Wextra -I. -Idata_structures/PCB \
  data_structures/PCB/pcb_test.c data_structures/PCB/Sch_PCB.c \
  -o outFiles/pcb_test.out

gcc -Wall -Wextra -I. -Idata_structures/PCB \
  process_generator/process_generator_queue_test.c process_generator/process_generator_functions.c \
  -o outFiles/pg_queue_test.out

gcc -Wall -Wextra -I. \
  process/process_test.c \
  -o outFiles/process_test.out

echo
echo "== Running tests =="
run_step "PCB queue test" ./outFiles/pcb_test.out
run_step "Process generator queue test" ./outFiles/pg_queue_test.out
run_step "Process integration test" ./outFiles/process_test.out

echo "== Summary =="
echo "Passed $PASS_COUNT/$TOTAL_COUNT tests"
