#
# Makefile for the STMicroelectronics Overdrive specific parts of the kernel
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

all: overdrive.o

clean:

O_TARGET := overdrive.o
obj-y	 := mach.o setup.o io.o led.o

ifneq ($(CONFIG_SH_GENERIC)$(CONFIG_SH_7750_OVERDRIVE),)
obj-$(CONFIG_PCI) +=  irq.o fpga.o galileo.o
endif

include $(TOPDIR)/Rules.make
