#
# Makefile for the FPU instruction emulation.
#

CFLAGS += -I. -I$(TOPDIR)/include/math-emu -w

ifeq ($(CONFIG_MATHEMU),y)

O_TARGET	:= math-emu.o
obj-y		:= math.o qrnnd.o

else

list-multi	:= math-emu.o
math-emu-objs	:= math.o qrnnd.o
obj-m		:= math-emu.o
math-emu.o: $(math-emu-objs)
	$(LD) -r -o $@ $(math-emu-objs)

endif

include $(TOPDIR)/Rules.make
