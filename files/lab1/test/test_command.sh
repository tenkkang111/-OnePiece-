#!/usr/bin/env bash

set -euo pipefail

fail() { echo "FAIL: $*" >&2; exit 1; }
pass() { echo "PASS: $*"; }

assert_dir()    { [[ -d "$1" ]] || fail "dir missing: $1"; }
assert_nodir()  { [[ ! -d "$1" ]] || fail "dir should NOT exist: $1"; }
assert_file()   { [[ -f "$1" ]] || fail "file missing: $1"; }
assert_nofile() { [[ ! -f "$1" ]] || fail "file should NOT exist: $1"; }

assert_file_allow_newline() {
  # Usage: assert_file_allow_newline <file> <expected-string>
  local file="$1" expected="$2"
  [[ -f "$file" ]] || fail "file missing: $file"

  # actual content (strip trailing newline if present)
  local got
  got="$(<"$file")"
  got="${got%$'\n'}"

  if [[ "$got" != "$expected" ]]; then
    echo "Content mismatch in $file" >&2
    echo "expected: '$expected'" >&2
    echo "got:      '$got'" >&2
    exit 1
  fi
}

### Checks start here

# 1) lab1_1 exists at repository root
assert_dir "lab1_1"

# 2) hello_linux directory + hello.txt
assert_dir  "lab1_1/hello_linux"
assert_file "lab1_1/hello_linux/hello.txt"

# 3) tmp directory is removed and tmp.txt has been moved out
assert_nodir  "lab1_1/tmp"
assert_nofile "lab1_1/tmp/tmp.txt"

# 4) work/tmp.txt exists and contains "tmp" (newline allowed)
assert_file "lab1_1/work/tmp.txt"
assert_file_allow_newline "lab1_1/work/tmp.txt" "tmp"

# 5) work/work.txt exists and contains "done" (newline allowed)
assert_file "lab1_1/work/work.txt"
assert_file_allow_newline "lab1_1/work/work.txt" "done"

# 6) trash directory and its files are removed
assert_nodir  "lab1_1/trash"
assert_nofile "lab1_1/trash/trash1.txt"
assert_nofile "lab1_1/trash/trash2.txt"

pass "Final state matches all requirements."

echo "SUCCESS: All tests passed!"
exit 0
