CC = riscv64-unknown-elf-gcc
AS = riscv64-unknown-elf-as
LD = riscv64-unknown-elf-ld
OBJCOPY = riscv64-unknown-elf-objcopy

CFLAGS = -march=rv64imac -mabi=lp64 -O2 -g -ffreestanding -nostdlib -fpic
ASFLAGS = -march=rv64imac
LDFLAGS = -T linker.ld -nostdlib

SOURCES = entry.o kernel.o memory.o task.o ipc.o posix_syscalls.o vm.o vfs.o
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

vfs.o: vfs.c vfs.h
	$(CC) $(CFLAGS) -c -o $@ $<

vmlinux.riscv64: $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $^

clean:
	rm -f *.o vmlinux.riscv64

.PHONY: all clean