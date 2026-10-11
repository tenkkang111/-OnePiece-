#!/bin/bash

TEST_BINARY="./who3"
SOURCE_FILES="who3.c utmplib.c"
REF_OUTPUT="${LAB_HOME}/test/who3_ref_output.txt"
TEST_OUTPUT="${LAB_HOME}/test/test_output.txt"

# Set timezone
export TZ=Asia/Seoul

# NOTE: Test input must be generated separately for each CPU architecture,
# since the layout of the utmp struct can differ between architectures.
#
# You can generate architecture-specific utmp files using the `utmpdump` tool.
# For example:
#   On an x86 machine:
#     cp /var/run/utmp test_utmp.x86
#     utmpdump test_utmp.x86 > test_utmp.tmp
#
#   Then, on an ARM machine:
#     utmpdump -r test_utmp.tmp > test_utmp.arm
#
# This converts the x86 binary dump into a format that matches the ARM utmp struct layout.

# Detect CPU architecture and set input file accordingly
ARCH=$(uname -m)
if [[ "$ARCH" == "x86_64" ]]; then
	TEST_INPUT="${LAB_HOME}/test/test_utmp.x86"
elif [[ "$ARCH" == "aarch64" || "$ARCH" == "arm64" ]]; then
	TEST_INPUT="${LAB_HOME}/test/test_utmp.arm"
else
	echo "ERROR: Unsupported CPU architecture: $ARCH"
	exit 1
fi

# Required tools check
REQUIRED_TOOLS=("gcc" "strace")
for tool in "${REQUIRED_TOOLS[@]}"; do
    if ! command -v $tool &> /dev/null; then
        echo "ERROR: '$tool' is not installed. Please install it before running the script."
	echo "HINT: sudo apt update; sudo apt install -y $tool"
        exit 1
    fi
done

# Check if source files exist
for file in $SOURCE_FILES; do
    if [ ! -f "$file" ]; then
        echo "ERROR: Source file $file not found."
        exit 1
    fi
done

# Set test utmp input as UTMP_FILE
cp who3.c who3.c.bak
sed -i '/^[[:space:]]*#include[[:space:]]*<utmp.h>/a\
  #undef UTMP_FILE\
  #define UTMP_FILE "'${TEST_INPUT}'"' who3.c

# Cleanup function to remove temporary files
cleanup() {
    rm -f "$TEST_OUTPUT"
    mv who3.c.bak who3.c
}

# Ensure cleanup runs on exit (normal or error)
trap cleanup EXIT

# Compile the test binary
gcc -o $TEST_BINARY $SOURCE_FILES 
if [ $? -ne 0 ]; then
    echo "ERROR: Compilation failed."
    exit 1
fi

# Check if test binaries exist
if [ ! -f "$TEST_BINARY" ]; then
    echo "ERROR: Test binary ($TEST_BINARY) not found after compilation."
    exit 1
fi

# Run test binary and compare output
$TEST_BINARY $TEST_INPUT > "$TEST_OUTPUT"

if ! diff -q "$REF_OUTPUT" "$TEST_OUTPUT" > /dev/null; then
    echo "FAILED: Output differs from the reference."
    echo "Reference output:" 
    cat $REF_OUTPUT
    echo "Your output:"
    cat $TEST_OUTPUT
    exit 1
fi

# Check the number of 'read' system calls using strace
REF_READ_COUNT="5"
TEST_READ_COUNT=$(strace -c $TEST_BINARY $TEST_INPUT 2>&1 | grep -w read | awk '{print $4}')

if [ "$REF_READ_COUNT" -ne "$TEST_READ_COUNT" ]; then
    echo "FAILED: read() system call count differs."
    echo "Reference output:" 
    echo $REF_READ_COUNT
    echo "Your output:"
    echo $TEST_READ_COUNT
    exit 1
fi

echo "SUCCESS: All tests passed!"
exit 0

