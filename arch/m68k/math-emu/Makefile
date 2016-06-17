#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

.S.o:
	$(CC) $(EXTRA_CFLAGS) $(AFLAGS) -traditional -c $< -o $*.o

#EXTRA_CFLAGS=-DFPU_EMU_DEBUG

O_TARGET := mathemu.o

obj-y		:= fp_entry.o fp_scan.o fp_util.o fp_move.o fp_movem.o \
			fp_cond.o fp_arith.o fp_log.o fp_trig.o

include $(TOPDIR)/Rules.make
