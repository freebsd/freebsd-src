#
# Makefile for the DMA sound driver
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

O_TARGET = dmasound.o

export-objs := dmasound_core.o

list-multi := dmasound_pmac.o

dmasound_pmac-objs := dmasound_awacs.o trans_16.o tas3001c.o dac3550a.o

obj-$(CONFIG_DMASOUND)        += dmasound_core.o
obj-$(CONFIG_DMASOUND_ATARI)  += dmasound_atari.o
obj-$(CONFIG_DMASOUND_PMAC)   += dmasound_pmac.o
obj-$(CONFIG_DMASOUND_PAULA)  += dmasound_paula.o
obj-$(CONFIG_DMASOUND_Q40)    += dmasound_q40.o

include $(TOPDIR)/Rules.make

dmasound_pmac.o: $(dmasound_pmac-objs)
	$(LD) $(EXTRA_LDFLAGS) -r -o $@ $(dmasound_pmac-objs)
