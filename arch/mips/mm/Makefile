#
# Makefile for the Linux/MIPS-specific parts of the memory manager.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

USE_STANDARD_AS_RULE := true

O_TARGET := mm.o

export-objs			+= cache.o ioremap.o loadmmu.o remap.o
obj-y				+= cache.o extable.o init.o ioremap.o fault.o \
				   loadmmu.o

obj-$(CONFIG_CPU_R3000)		+= pg-r4k.o c-r3k.o tlb-r3k.o tlbex-r3k.o
obj-$(CONFIG_CPU_TX39XX)	+= pg-r4k.o c-tx39.o tlb-r3k.o tlbex-r3k.o
obj-$(CONFIG_CPU_TX49XX)	+= pg-r4k.o c-r4k.o tlb-r4k.o tlbex-r4k.o
obj-$(CONFIG_CPU_R4300)		+= pg-r4k.o c-r4k.o tlb-r4k.o tlbex-r4k.o
obj-$(CONFIG_CPU_R4X00)		+= pg-r4k.o c-r4k.o tlb-r4k.o tlbex-r4k.o
obj-$(CONFIG_CPU_VR41XX)	+= pg-r4k.o c-r4k.o tlb-r4k.o tlbex-r4k.o
obj-$(CONFIG_CPU_R5000)		+= pg-r4k.o c-r4k.o tlb-r4k.o tlbex-r4k.o
obj-$(CONFIG_CPU_NEVADA)	+= pg-r4k.o c-r4k.o tlb-r4k.o tlbex-r4k.o
obj-$(CONFIG_CPU_R5432)		+= pg-r4k.o c-r4k.o tlb-r4k.o tlbex-r4k.o
obj-$(CONFIG_CPU_RM7000)	+= pg-r4k.o c-r4k.o tlb-r4k.o tlbex-r4k.o
obj-$(CONFIG_CPU_RM9000)	+= pg-r4k.o c-r4k.o tlb-r4k.o tlbex-r4k.o
obj-$(CONFIG_CPU_R10000)	+= pg-r4k.o c-r4k.o tlb-r4k.o tlbex-r4k.o
obj-$(CONFIG_CPU_MIPS32)	+= pg-r4k.o c-r4k.o tlb-r4k.o tlbex-mips32.o
obj-$(CONFIG_CPU_MIPS64)	+= pg-r4k.o c-r4k.o tlb-r4k.o tlbex-r4k.o
obj-$(CONFIG_CPU_SB1)		+= pg-sb1.o c-sb1.o tlb-sb1.o tlbex-r4k.o \
				   cex-sb1.o cerr-sb1.o

obj-$(CONFIG_64BIT_PHYS_ADDR)	+= remap.o
obj-$(CONFIG_RM7000_CPU_SCACHE)	+= sc-rm7k.o
obj-$(CONFIG_R5000_CPU_SCACHE)	+= sc-r5k.o
obj-$(CONFIG_SGI_IP22)		+= sc-ip22.o

include $(TOPDIR)/Rules.make
