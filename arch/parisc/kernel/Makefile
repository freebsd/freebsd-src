#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

ifdef CONFIG_PARISC64
all: kernel.o init_task.o pdc_cons.o process.o head64.o unaligned.o perf.o perf_asm.o
else
all: kernel.o init_task.o pdc_cons.o process.o head.o unaligned.o
endif

O_TARGET = kernel.o

obj-y		:=
obj-m		:=
obj-n		:=
obj-		:=

obj-y           += cache.o pacache.o setup.o traps.o time.o irq.o \
		pa7300lc.o syscall.o entry.o sys_parisc.o firmware.o \
		ptrace.o hardware.o inventory.o drivers.o semaphore.o \
		signal.o hpmc.o real2.o parisc_ksyms.o unaligned.o \
		processor.o power.o pdc_chassis.o

export-objs	:= parisc_ksyms.o superio.o keyboard.o


obj-$(CONFIG_SMP) += smp.o irq_smp.o
obj-$(CONFIG_PA11) += pci-dma.o
obj-$(CONFIG_PCI) += pci.o
obj-$(CONFIG_VT) += keyboard.o
obj-$(CONFIG_PCI_LBA) += lba_pci.o
# I/O SAPIC is also on IA64 platforms.
# The two could be merged into a common source some day.
obj-$(CONFIG_IOSAPIC) += iosapic.o
obj-$(CONFIG_IOMMU_SBA) += sba_iommu.o
# Only use one of them: ccio-rm-dma is for PCX-W systems *only*
# obj-$(CONFIG_IOMMU_CCIO) += ccio-rm-dma.o
obj-$(CONFIG_IOMMU_CCIO) += ccio-dma.o
obj-$(CONFIG_CHASSIS_LCD_LED) += led.o 
obj-$(CONFIG_SUPERIO) += superio.o
obj-$(CONFIG_PARISC64) += binfmt_elf32.o sys_parisc32.o \
		ioctl32.o signal32.o
# only supported for PCX-W/U in 64-bit mode at the moment
obj-$(CONFIG_PARISC64) += perf.o perf_asm.o

include $(TOPDIR)/Rules.make
