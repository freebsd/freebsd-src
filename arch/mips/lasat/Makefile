#
# Makefile for the LASAT specific kernel interface routines under Linux.
#
USE_STANDARD_AS_RULE := 1

O_TARGET := lasatkern.o
obj-y	 := reset.o setup.o prom.o lasat_board.o \
		at93c.o interrupt.o lasatIRQ.o

obj-$(CONFIG_LASAT_SYSCTL) += sysctl.o

obj-$(CONFIG_DS1603) += ds1603.o

obj-$(CONFIG_PICVUE) += picvue.o
obj-$(CONFIG_PICVUE_PROC) += picvue_proc.o

obj-$(CONFIG_PCI) += pci.o

include $(TOPDIR)/Rules.make

clean:
	make -C image clean
