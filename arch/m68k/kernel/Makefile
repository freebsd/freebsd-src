#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

.S.o:
	$(CC) $(AFLAGS) -traditional -c $< -o $*.o

ifndef CONFIG_SUN3
all: head.o kernel.o
else
all: sun3-head.o kernel.o
endif 

O_TARGET := kernel.o

export-objs	:= setup.o m68k_ksyms.o

obj-y		:= entry.o process.o traps.o ints.o signal.o ptrace.o \
			sys_m68k.o time.o semaphore.o setup.o m68k_ksyms.o

obj-$(CONFIG_PCI)	+= bios32.o

head.o: head.S m68k_defs.h

entry.o: entry.S m68k_defs.h

sun3-head.o: sun3-head.S m68k_defs.h

m68k_defs.h: m68k_defs.c m68k_defs.head
	rm -f m68k_defs.d
	SUNPRO_DEPENDENCIES="m68k_defs.d m68k_defs.h" \
	$(CC) $(filter-out -MD,$(CFLAGS)) -S m68k_defs.c
	cp m68k_defs.head m68k_defs.h
	grep '^#define' m68k_defs.s >> m68k_defs.h
	rm m68k_defs.s
-include m68k_defs.d

include $(TOPDIR)/Rules.make
