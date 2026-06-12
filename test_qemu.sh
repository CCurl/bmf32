#!/bin/bash
# Test BMF bare metal kernel in QEMU

echo "Testing BMF Bare Metal Kernel in QEMU..."
echo "Expected: Serial output from bootloader"
echo "---"

rm -f /tmp/qemu_serial.log /tmp/qemu_output.txt

# Run QEMU with serial output to file
qemu-system-i386 \
  -kernel kernel.elf \
  -serial file:/tmp/qemu_serial.log \
  -nographic \
  -m 64 \
  -monitor none \
  -d int \
  2>/tmp/qemu_errors.log &

QEMU_PID=$!

# Wait for kernel to initialize
sleep 2

# Kill QEMU
kill $QEMU_PID 2>/dev/null
wait $QEMU_PID 2>/dev/null || true

echo ""
echo "=== Serial Output ==="
if [ -f /tmp/qemu_serial.log ]; then
    cat /tmp/qemu_serial.log
    echo ""
else
    echo "(No serial output file)"
fi

echo ""
echo "=== QEMU Errors/Debug Output ==="
if [ -f /tmp/qemu_errors.log ]; then
    head -50 /tmp/qemu_errors.log
else
    echo "(No error log)"
fi

