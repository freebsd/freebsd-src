#
# Makefile for most of the non-PCI devices in PA-RISC machines
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now inherited from the
# parent makefile.
#

O_TARGET := gscbus.o

obj-y := gsc.o
obj-m :=
obj-n :=
obj-  :=

obj-$(CONFIG_GSC_DINO) += dino.o
obj-$(CONFIG_GSC_LASI) += lasi.o asp.o
obj-$(CONFIG_GSC_WAX) += wax.o
obj-$(CONFIG_SERIAL_GSC) += serial.o
obj-$(CONFIG_EISA) += eisa.o eisa_enumerator.o eisa_eeprom.o

include $(TOPDIR)/Rules.make
