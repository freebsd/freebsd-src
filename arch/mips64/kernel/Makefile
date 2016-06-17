#
# Makefile for the Linux/MIPS kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

all:	kernel.o head.o init_task.o

O_TARGET := kernel.o

export-objs	= irq.o mips64_ksyms.o pci-dma.o setup.o semaphore.o smp.o time.o

obj-y		:= branch.o cpu-probe.o entry.o irq.o proc.o process.o \
		   ptrace.o r4k_cache.o r4k_fpu.o r4k_genex.o r4k_switch.o \
		   reset.o scall_64.o semaphore.o setup.o signal.o syscall.o \
		   traps.o unaligned.o

obj-$(CONFIG_I8259)		+= i8259.o
obj-$(CONFIG_IRQ_CPU)		+= irq_cpu.o
obj-$(CONFIG_NEW_TIME_C)	+= time.o

obj-$(CONFIG_MODULES)		+= mips64_ksyms.o
obj-$(CONFIG_MIPS32_COMPAT)	+= linux32.o signal32.o ioctl32.o
obj-$(CONFIG_MIPS32_N32)	+= binfmt_elfn32.o scall_n32.o signal_n32.o
obj-$(CONFIG_MIPS32_O32)	+= binfmt_elfo32.o scall_o32.o
obj-$(CONFIG_SMP)		+= smp.o

obj-$(CONFIG_KGDB)		+= gdb-low.o gdb-stub.o

ifndef CONFIG_MAPPED_PCI_IO
obj-y				+= pci-dma.o
endif

CFLAGS_cpu-probe.o	= $(shell if $(CC) $(CFLAGS) -Wa,-mdaddi -c -o /dev/null -xc /dev/null >/dev/null 2>&1; then echo "-DHAVE_AS_SET_DADDI"; fi)
AFLAGS_r4k_genex.o	= -P

include $(TOPDIR)/Rules.make
