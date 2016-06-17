#
# Makefile for the Linux kernel pci hotplug controller drivers.
#

O_TARGET	:= vmlinux-obj.o

list-multi	:= cpqphp.o pci_hotplug.o ibmphp.o acpiphp.o 

export-objs	:= pci_hotplug_core.o pci_hotplug_util.o

obj-$(CONFIG_HOTPLUG_PCI)		+= pci_hotplug.o
obj-$(CONFIG_HOTPLUG_PCI_COMPAQ)	+= cpqphp.o
obj-$(CONFIG_HOTPLUG_PCI_IBM)		+= ibmphp.o
obj-$(CONFIG_HOTPLUG_PCI_ACPI)		+= acpiphp.o

pci_hotplug-objs	:=	pci_hotplug_core.o	\
				pci_hotplug_util.o

cpqphp-objs		:=	cpqphp_core.o	\
				cpqphp_ctrl.o	\
				cpqphp_proc.o	\
				cpqphp_pci.o

ibmphp-objs		:=	ibmphp_core.o	\
				ibmphp_ebda.o	\
				ibmphp_pci.o	\
				ibmphp_res.o	\
				ibmphp_hpc.o

acpiphp_objs		:=	acpiphp_core.o	\
				acpiphp_glue.o	\
				acpiphp_pci.o	\
				acpiphp_res.o

ifeq ($(CONFIG_HOTPLUG_PCI_COMPAQ_NVRAM),y)
	cpqphp-objs += cpqphp_nvram.o
endif


include $(TOPDIR)/Rules.make

pci_hotplug.o: $(pci_hotplug-objs)
	$(LD) -r -o $@ $(pci_hotplug-objs)

cpqphp.o: $(cpqphp-objs)
	$(LD) -r -o $@ $(cpqphp-objs)

ibmphp.o: $(ibmphp-objs)
	$(LD) -r -o $@ $(ibmphp-objs)

acpiphp.o: $(acpiphp_objs)
	$(LD) -r -o $@ $(acpiphp_objs)
