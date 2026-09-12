#!/usr/bin/env bash
# Builds and runs the full test suite in main.c, then fails (nonzero exit)
# unless every test passed. Optionally also runs it under AddressSanitizer
# + UndefinedBehaviorSanitizer if `--sanitize` is passed, and under
# ThreadSanitizer if `--tsan` is passed, to catch memory-safety and data-race
# bugs that a plain pass/fail count can hide.
set -euo pipefail
cd "$(dirname "$0")"

make >/dev/null
echo "== plain build =="
out=$(./myalloc)
echo "$out"
echo "$out" | grep -qE '^----TEST_RESULT_Success: ([0-9]+)/\1$' || {
  echo "FAILED: not all tests passed" >&2
  exit 1
}

if [[ "${1:-}" == "--sanitize" || "${1:-}" == "--all" ]]; then
  echo
  echo "== AddressSanitizer + UndefinedBehaviorSanitizer =="
  gcc -Wall -g -std=c99 -D_POSIX_C_SOURCE=199309L -fsanitize=address,undefined -pthread main.c myalloc.c -o /tmp/myalloc_asan
  /tmp/myalloc_asan
fi

if [[ "${1:-}" == "--tsan" || "${1:-}" == "--all" ]]; then
  echo
  echo "== ThreadSanitizer (test_threading only; case/test = 0 3) =="
  gcc -Wall -g -std=c99 -D_POSIX_C_SOURCE=199309L -fsanitize=thread -pthread main.c myalloc.c -o /tmp/myalloc_tsan
  /tmp/myalloc_tsan 0 3
fi

echo
echo "All good."
