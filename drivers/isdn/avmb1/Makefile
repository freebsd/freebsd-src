# Makefile for the AVM ISDN device drivers and CAPI subsystem.

# The target object and module list name.

O_TARGET	:= vmlinux-obj.o

# Objects that export symbols.

export-objs	:= kcapi.o capiutil.o b1dma.o b1pcmcia.o b1.o capifs.o

# Multipart objects.

list-multi	:= kernelcapi.o
kernelcapi-objs	:= kcapi.o

# Ordering constraints: kernelcapi.o first

# Each configuration option enables a list of files.

obj-$(CONFIG_ISDN_CAPI)			+= kernelcapi.o capiutil.o
obj-$(CONFIG_ISDN_CAPI_CAPI20)		+= capi.o 
obj-$(CONFIG_ISDN_CAPI_CAPIDRV)		+= capidrv.o
obj-$(CONFIG_ISDN_CAPI_CAPIFS)		+= capifs.o
obj-$(CONFIG_ISDN_DRV_AVMB1_B1ISA)	+= b1isa.o b1.o
obj-$(CONFIG_ISDN_DRV_AVMB1_B1PCI)	+= b1pci.o b1.o b1dma.o
obj-$(CONFIG_ISDN_DRV_AVMB1_B1PCMCIA)	+= b1pcmcia.o b1.o
obj-$(CONFIG_ISDN_DRV_AVMB1_AVM_CS)	+= avm_cs.o
obj-$(CONFIG_ISDN_DRV_AVMB1_T1ISA)	+= t1isa.o b1.o
obj-$(CONFIG_ISDN_DRV_AVMB1_T1PCI)	+= t1pci.o b1.o b1dma.o
obj-$(CONFIG_ISDN_DRV_AVMB1_C4)		+= c4.o b1.o

# The global Rules.make.

include $(TOPDIR)/Rules.make

# Link rules for multi-part drivers.

kernelcapi.o: $(kernelcapi-objs)
	$(LD) -r -o $@ $(kernelcapi-objs)

