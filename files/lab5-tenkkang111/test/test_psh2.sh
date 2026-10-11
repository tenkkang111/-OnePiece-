#!/bin/bash

TARGET="psh2"
SRC="./${TARGET}.c"
BIN="./${TARGET}"
OUTPUT_DIR="outputs_psh2"

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
# Function to run test using expect
############################################
run_expect() {
  local test_name="$1"
  shift
  local inputs=("$@")
  local outfile="${OUTPUT_DIR}/${test_name}.log"

  expect <<EOF > /dev/null
    set timeout 1
    log_file -noappend "$outfile"
    spawn $BIN
    set inputs [list ${inputs[@]}]
    foreach input \$inputs {
      expect {
        "Arg*" { send "\$input\r" }
      }
    }
    send "\r"   ;# Trigger execution
    expect eof
EOF
}

############################################
# Test 1: Run 'echo hello'
############################################
echo "=== Test 1: echo hello ==="
run_expect "test1_echo" "{echo} {hello}"
if grep -q "hello" "${OUTPUT_DIR}/test1_echo.log"; then
  echo "PASS: echo hello output verified."
else
  echo "FAIL: echo hello output missing."
  exit 1
fi

############################################
# Test 2: Invalid command
############################################
echo "=== Test 2: Invalid Command ==="
run_expect "test2_invalid" "{invalidcmd}"
if grep -q "execvp failed" "${OUTPUT_DIR}/test2_invalid.log"; then
  echo "PASS: Invalid command error detected."
else
  echo "FAIL: Missing error message for invalid command."
  exit 1
fi

############################################
# Test 3: List directory (ls)
############################################
echo "=== Test 3: ls ==="
run_expect "test3_ls" "{ls}" "$SRC"
if grep -q "$(basename $SRC)" "${OUTPUT_DIR}/test3_ls.log"; then
  echo "PASS: ls output contains source file."
else
  echo "FAIL: ls output incorrect."
  exit 1
fi

############################################
# Test 4: Child exit status check
############################################
echo "=== Test 4: Child Exit Status ==="
if grep -q "Child exited with status 0" "${OUTPUT_DIR}/test1_echo.log"; then
  echo "PASS: Correct child exit status reported."
else
  echo "FAIL: Incorrect child exit status."
  exit 1
fi

############################################
# Test 5: Custom Exec Test
############################################
echo "=== Test 5: Custom Exec Program ==="

# Compile the test_exec program
gcc -o ${LAB_HOME}/test/test_exec ${LAB_HOME}/test/test_exec.c
if [[ $? -ne 0 ]]; then
  echo "FAIL: test_exec build failed."
  exit 1
fi

run_expect "test5_custom_exec" ./test/test_exec

# Check execution message
if grep -q "I was executed!" "${OUTPUT_DIR}/test5_custom_exec.log"; then
  echo "PASS: Custom program executed."
else
  echo "FAIL: Custom program did not execute properly."
  exit 1
fi

# Check exit status 42
if grep -q "Child exited with status 42" "${OUTPUT_DIR}/test5_custom_exec.log"; then
  echo "PASS: Correct exit status detected."
else
  echo "FAIL: Incorrect exit status for custom program."
  exit 1
fi

############################################
# Test 6: Multiple Commands Execution (echo first, second)
############################################
echo "=== Test 6: Multiple Commands Execution ==="

expect <<EOF > /dev/null
  set timeout 3
  log_file -noappend "${OUTPUT_DIR}/test6_multiple_commands.log"

  spawn $BIN

  # --- First Command: echo first ---
  if {[catch {
      expect -re "Arg\\[0\\]\\?"
      send "echo\r"

      expect -re "Arg\\[1\\]\\?"
      send "first\r"

      expect -re "Arg\\[2\\]\\?"
      send "\r"
    } err]} {
      puts "FAIL: Error during first command input: \$err"
      exit 1
    }

  if {[catch {
      expect {
        -re "\nfirst" {}
        timeout { puts "FAIL: Missing output for 'echo first'."; exit 1 }
      }

      expect {
        -re "Child exited with status 0, signal 0" {}
        timeout { puts "FAIL: Missing child exit message after 'echo first'."; exit 1 }
      }
    } err]} {
      puts "FAIL: Error during first command execution: \$err"
      exit 1
    }

  # --- Second Command: echo second ---
  if {[catch {
      expect -re "Arg\\[0\\]\\?"
      send "echo\r"

      expect -re "Arg\\[1\\]\\?"
      send "second\r"

      expect -re "Arg\\[2\\]\\?"
      send "\r"
    } err]} {
      puts "FAIL: Error during second command input: \$err"
      exit 1
    }

  if {[catch {
      expect {
        -re "\nsecond" {}
        timeout { puts "FAIL: Missing output for 'echo second'."; exit 1 }
      }

      expect {
        -re "Child exited with status 0, signal 0" {}
        timeout { puts "FAIL: Missing child exit message after 'echo second'."; exit 1 }
      }
    } err]} {
      puts "FAIL: Error during second command execution: \$err"
      exit 1
    }

  if {[catch {
      expect -re "Arg\\[0\\]\\?"
      expect eof
    } err]} {
      puts "FAIL: Error during final prompt handling: \$err"
      exit 1
    }
EOF

LOG_FILE="${OUTPUT_DIR}/test6_multiple_commands.log"

if grep -q "first" "$LOG_FILE" && \
  grep -q "second" "$LOG_FILE" && \
  [ "$(grep -c "Child exited with status 0, signal 0" "$LOG_FILE")" -eq 2 ]; then
  echo "PASS: Both commands executed successfully with correct outputs."
else
  echo "FAIL: Missing outputs or child exit messages."
  exit 1
fi


echo ""
echo "SUCCESS: All tests PASS!"

