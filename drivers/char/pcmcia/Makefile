#
# drivers/char/pcmcia/Makefile
#
# Makefile for the Linux PCMCIA char device drivers.
#

SUB_DIRS     := 
MOD_SUB_DIRS := $(SUB_DIRS)
ALL_SUB_DIRS := $(SUB_DIRS)

O_TARGET := pcmcia_char.o

obj-y		:=
obj-m		:=
obj-n		:=
obj-		:=

obj-$(CONFIG_PCMCIA_SERIAL_CS)	+= serial_cs.o
obj-$(CONFIG_SYNCLINK_CS) += synclink_cs.o

include $(TOPDIR)/Rules.make
