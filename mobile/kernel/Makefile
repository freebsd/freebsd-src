CC = riscv64-unknown-elf-gcc
AS = riscv64-unknown-elf-as
LD = riscv64-unknown-elf-ld
OBJCOPY = riscv64-unknown-elf-objcopy

CFLAGS = -march=rv64imac_zicsr_zifencei -mabi=lp64 -O2 -g -ffreestanding -nostdlib -fpic
ASFLAGS = -march=rv64imac_zicsr_zifencei
LDFLAGS = -T linker.ld -nostdlib

SOURCES = entry.o kernel.o memory.o task.o ipc.o posix_syscalls.o vm.o context_switch.o chardev.o interrupt.o buffer_cache.o vfs.o blockdev.o hybridfs.o virtio_blk.o uart.o page_cache.o virtio_net.o memory_utils.o pmp.o syscall_security.o aslr.o stack_canary.o framebuffer.o ui_widget.o window_manager.o mobile_ui.o ui_animation.o
OBJECTS = $(SOURCES)

all: vmlinux.riscv64

entry.o: entry.S
	$(AS) $(ASFLAGS) -o $@ $<

kernel.o: kernel.c
	$(CC) $(CFLAGS) -c -o $@ $<

memory.o: memory.c memory.h
	$(CC) $(CFLAGS) -c -o $@ $<

task.o: task.c task.h
	$(CC) $(CFLAGS) -c -o $@ $<

ipc.o: ipc.c ipc.h
	$(CC) $(CFLAGS) -c -o $@ $<

posix_syscalls.o: posix_syscalls.c posix_syscalls.h
	$(CC) $(CFLAGS) -c -o $@ $<

vm.o: vm.c vm.h
	$(CC) $(CFLAGS) -c -o $@ $<

context_switch.o: context_switch.S
	$(AS) $(ASFLAGS) -c -o $@ $<

chardev.o: chardev.c chardev.h
	$(CC) $(CFLAGS) -c -o $@ $<

interrupt.o: interrupt.c interrupt.h
	$(CC) $(CFLAGS) -c -o $@ $<

buffer_cache.o: buffer_cache.c buffer_cache.h
	$(CC) $(CFLAGS) -c -o $@ $<

vfs.o: vfs.c vfs.h
	$(CC) $(CFLAGS) -c -o $@ $<

blockdev.o: blockdev.c blockdev.h memory.h
	$(CC) $(CFLAGS) -c -o $@ $<

hybridfs.o: hybridfs.c hybridfs.h blockdev.h memory.h posix_syscalls.h vfs.h
	$(CC) $(CFLAGS) -c -o $@ $<

virtio_blk.o: ../drivers/virtio_blk.c ../drivers/virtio_blk.h memory.h
	$(CC) $(CFLAGS) -c -o $@ $<

uart.o: ../drivers/uart.c ../drivers/uart.h chardev.h
	$(CC) $(CFLAGS) -c -o $@ $<

page_cache.o: page_cache.c page_cache.h vfs.h memory.h blockdev.h
	$(CC) $(CFLAGS) -c -o $@ $<

virtio_net.o: ../drivers/virtio_net.c ../drivers/virtio_net.h chardev.h
	$(CC) $(CFLAGS) -c -o $@ $<

memory_utils.o: memory_utils.c memory_utils.h
	$(CC) $(CFLAGS) -c -o $@ $<

pmp.o: pmp.c pmp.h
	$(CC) $(CFLAGS) -c -o $@ $<

syscall_security.o: syscall_security.c syscall_security.h task.h
	$(CC) $(CFLAGS) -c -o $@ $<

aslr.o: aslr.c aslr.h
	$(CC) $(CFLAGS) -c -o $@ $<

stack_canary.o: stack_canary.c stack_canary.h
	$(CC) $(CFLAGS) -c -o $@ $<

framebuffer.o: ../ui/framebuffer.c ../ui/framebuffer.h
	$(CC) $(CFLAGS) -c -o $@ $<

ui_widget.o: ../ui/ui_widget.c ../ui/ui_widget.h ../ui/framebuffer.h
	$(CC) $(CFLAGS) -c -o $@ $<

window_manager.o: ../ui/window_manager.c ../ui/window_manager.h ../ui/ui_widget.h
	$(CC) $(CFLAGS) -c -o $@ $<

mobile_ui.o: ../ui/mobile_ui.c ../ui/mobile_ui.h ../ui/window_manager.h ../ui/ui_animation.h
	$(CC) $(CFLAGS) -c -o $@ $<

ui_animation.o: ../ui/ui_animation.c ../ui/ui_animation.h ../ui/ui_widget.h ../ui/window_manager.h
	$(CC) $(CFLAGS) -c -o $@ $<

vmlinux.riscv64: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

clean:
	rm -f *.o vmlinux.riscv64

.PHONY: all clean