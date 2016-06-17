#
# Makefile for the Linux/MIPS kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

O_TARGET 			:= pci-core.o

export-objs			= pci.o

obj-$(CONFIG_MIPS_BONITO64)	+= ops-bonito64.o
obj-$(CONFIG_MIPS_GT64120)	+= ops-gt64120.o
obj-$(CONFIG_MIPS_MSC)		+= ops-msc.o
obj-$(CONFIG_MIPS_NILE4)	+= ops-nile4.o
obj-$(CONFIG_SNI_RM200_PCI)	+= ops-sni.o
obj-y				+= pci.o
obj-$(CONFIG_PCI_AUTO)		+= pci_auto.o

include $(TOPDIR)/Rules.make
