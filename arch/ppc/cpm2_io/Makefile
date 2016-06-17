#
# Makefile for the linux CPM2 ppc-specific parts of comm processor
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now in the main makefile...

O_TARGET		:= cpm2_io.o

obj-y			:= commproc.o uart.o

obj-$(CONFIG_FEC_ENET)	+= fcc_enet.o
obj-$(CONFIG_SCC_ENET)	+= enet.o

include $(TOPDIR)/Rules.make
