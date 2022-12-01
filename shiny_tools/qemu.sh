#!/usr/bin/env bash

DEBUG_ARG=''
if [[ "$1" == "-d" ]]; then
	DEBUG_ARG='-S -s'
fi

echo "Starting QEMU (press ctr-A, X to terminate)..."
qemu-system-aarch64 -m 4096M -cpu cortex-a72 -smp 4 -M virt  \
        -bios /usr/lib/u-boot/qemu_arm64/u-boot.bin \
        -nographic -serial mon:stdio \
        -drive if=none,file=../qemu/test.img,id=hd0 \
        -device virtio-blk-device,drive=hd0 \
        -device virtio-net-device,netdev=net0 \
        -netdev user,id=net0 $DEBUG_ARG

QEMU_EXIT=$?
echo "QEMU exited (status = $QEMU_EXIT)"
exit $QEMU_EXIT

