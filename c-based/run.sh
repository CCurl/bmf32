#!/bin/bash
# Run script for Bare Metal OS

set -e

echo "Building Bare Metal OS..."
make clean
make

echo ""
echo "Launching QEMU..."
echo "Press Ctrl+A then X to exit QEMU"
echo ""

make run
