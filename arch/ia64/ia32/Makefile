#
# Makefile for the ia32 kernel emulation subsystem.
#

.S.s:
	$(CPP) $(AFLAGS) $(AFLAGS_KERNEL) -o $*.s $<
.S.o:
	$(CC) $(AFLAGS) $(AFLAGS_KERNEL) -c -o $*.o $<

all: ia32.o

O_TARGET := ia32.o

obj-y := ia32_entry.o sys_ia32.o ia32_ioctl.o ia32_signal.o ia32_support.o ia32_traps.o \
	 binfmt_elf32.o ia32_ldt.o

clean::

include $(TOPDIR)/Rules.make
