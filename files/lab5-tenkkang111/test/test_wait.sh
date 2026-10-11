#!/bin/bash

TARGET="wait"
SRC="./${TARGET}.c"
BIN="./${TARGET}"
OUTPUT_DIR="outputs_wait"

# Check if source file exists
if [[ ! -f "$SRC" ]]; then
  echo "FAIL: Source file '$SRC' not found"
  exit 1
fi

# Build the program
if ! gcc -o "$BIN" "$SRC" 2> build_error.log; then
  echo "FAIL: Build failed"
  cat build_error.log
  exit 1
fi
rm -f build_error.log

rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"

############################################
# Function to run the program and capture output
############################################
run_program() {
  local outfile="$1"
  $BIN > "$outfile"
}

############################################
# Test 1: Check output order & wait() behavior
############################################
echo "=== Test 1: Output Order & wait() Check ==="
OUT1="${OUTPUT_DIR}/test1.log"
run_program "$OUT1"

PARENT_LINE=$(grep "Before: my pid is" "$OUT1")
CHILD_LINE=$(grep "Child " "$OUT1" | head -n1)
WAIT_LINE=$(grep "Done waiting" "$OUT1")

PARENT_PID=$(echo "$PARENT_LINE" | awk '{print $5}')
CHILD_PID=$(echo "$CHILD_LINE" | awk '{print $2}')
WAIT_RET=$(echo "$WAIT_LINE" | awk -F'returned: ' '{print $2}')

if [[ "$WAIT_RET" == "$CHILD_PID" ]]; then
  echo "PASS: wait() returned correct child PID."
else
  echo "FAIL: wait() returned wrong PID. Expected $CHILD_PID but got $WAIT_RET."
  exit 1
fi

############################################
# Test 2: Check exit status values
############################################
echo "=== Test 2: Exit Status Check ==="
STATUS_LINE=$(grep "Status:" "$OUT1")

if echo "$STATUS_LINE" | grep -q "exit=17, signal=0, core dumped=0"; then
  echo "PASS: Exit status correctly reported."
else
  echo "FAIL: Exit status incorrect: $STATUS_LINE"
  exit 1
fi

############################################
# Test 3: Send SIGINT to child process
############################################
echo "=== Test 3: Signal Handling Check ==="
OUT2="${OUTPUT_DIR}/test3_signal.log"

# Run in foreground-like mode to maintain TTY behavior
(stdbuf -oL $BIN | tee "$OUT2") &
PARENT_PID=$!

sleep 1

CHILD_PID=$(grep "Child " "$OUT2" | head -n1 | awk '{print $2}')

if [[ -z "$CHILD_PID" ]]; then
  echo "FAIL: Could not detect child PID."
  exit 1
fi

# Check if child is still alive before sending signal
if ps -p "$CHILD_PID" > /dev/null; then
   kill -SIGINT "$CHILD_PID"
else
   echo "WARNING: Child process already exited before signal."
   exit 1
fi

wait "$PARENT_PID"

SIG_STATUS_LINE=$(grep "Status:" "$OUT2")

echo ""
if echo "$SIG_STATUS_LINE" | grep -q "exit=0, signal=2, core dumped=0"; then
  echo "PASS: Signal handling correctly reported."
else
  echo "FAIL: Signal handling incorrect: $SIG_STATUS_LINE"
  exit 1
fi

echo ""
echo "SUCCESS: All tests PASS!"

