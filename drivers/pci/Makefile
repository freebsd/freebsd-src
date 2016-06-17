#
# Makefile for the PCI bus specific drivers.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definition is now inherited from the
# parent makefile.
#

O_TARGET := driver.o

export-objs := pci.o

obj-$(CONFIG_PCI) += pci.o quirks.o compat.o names.o
obj-$(CONFIG_PROC_FS) += proc.o

ifndef CONFIG_SPARC64
obj-$(CONFIG_PCI) += setup-res.o
endif

#
# Some architectures use the generic PCI setup functions
#
obj-$(CONFIG_ALPHA) += setup-bus.o setup-irq.o
obj-$(CONFIG_ARM) += setup-bus.o setup-irq.o
obj-$(CONFIG_PARISC) += setup-bus.o
obj-$(CONFIG_SUPERH) += setup-bus.o setup-irq.o
obj-$(CONFIG_PPC32) += setup-irq.o
obj-$(CONFIG_SGI_IP27) += setup-irq.o

ifndef CONFIG_X86
obj-y += syscall.o
endif

include $(TOPDIR)/Rules.make

names.o: names.c devlist.h classlist.h

devlist.h classlist.h: pci.ids gen-devlist
	./gen-devlist <pci.ids

gen-devlist: gen-devlist.c
	$(HOSTCC) $(HOSTCFLAGS) -o gen-devlist gen-devlist.c
