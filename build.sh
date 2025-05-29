#!/bin/bash

CXX=g++
CXXFLAGS="-O3"
LLVM_FLAGS=$(llvm-config --cxxflags --ldflags --system-libs --libs core)

SRC_DIR="src"

OUTPUT_EXEC="passman"

SOURCE_FILES=$(find "$SRC_DIR" -name "*.cpp" | sort)

echo "Found sources:"
echo "$SOURCE_FILES" | sed 's/^/  /'
echo ""

echo "$CXX $CXXFLAGS $SOURCE_FILES $LLVM_FLAGS -o $OUTPUT_EXEC"

$CXX $CXXFLAGS $SOURCE_FILES $LLVM_FLAGS -o $OUTPUT_EXEC

if [ $? -eq 0 ]; then
    echo ""
    echo "success: $OUTPUT_EXEC"
else
    echo ""
    echo "fail"
    exit 1
fi