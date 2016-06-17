#
# Makefile for the ia32 kernel emulation subsystem.
#

.S.s:
	$(CPP) $(AFLAGS) -o $*.s $<
.S.o:
	$(CC) $(AFLAGS) -c -o $*.o $<

export-objs := ia32_ioctl.o	

all: ia32.o

O_TARGET := ia32.o
obj-$(CONFIG_IA32_EMULATION) := ia32entry.o sys_ia32.o ia32_ioctl.o ia32_signal.o \
	ia32_binfmt.o fpu32.o socket32.o ptrace32.o ipc32.o

clean::

include $(TOPDIR)/Rules.make
