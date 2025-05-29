#!/bin/bash

CXX=g++
CXXFLAGS="-O3"
LLVM_FLAGS="`llvm-config --cxxflags --ldflags --system-libs --libs core`"
CUSTOM_FLAGS="-DPRINT_STATS"

SRC_DIR="src"

OUTPUT_EXEC="passman"

SOURCE_FILES=$(find "$SRC_DIR" -name "*.cpp")

echo "Found sources:"
echo "$SOURCE_FILES" | sed 's/^/  /'
echo ""

echo "$CXX $CXXFLAGS $CUSTOM_FLAGS $SOURCE_FILES $LLVM_FLAGS -o $OUTPUT_EXEC"

$CXX $CXXFLAGS $CUSTOM_FLAGS $SOURCE_FILES $LLVM_FLAGS -o $OUTPUT_EXEC

if [ $? -eq 0 ]; then
    echo ""
    echo "success: $OUTPUT_EXEC"
else
    echo ""
    echo "fail"
    exit 1
fi