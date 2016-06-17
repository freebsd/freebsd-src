#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

.S.o:
	$(CC) $(AFLAGS) -traditional -c $< -o $*.o

all: kernel.o head.o head64.o init_task.o

O_TARGET := kernel.o

export-objs     := mtrr.o msr.o cpuid.o x8664_ksyms.o pci-gart.o setup.o swiotlb.o

obj-y	:= process.o semaphore.o signal.o entry.o traps.o irq.o \
		ptrace.o i8259.o ioport.o ldt.o setup.o time.o sys_x86_64.o \
		pci-dma.o x8664_ksyms.o i387.o syscall.o vsyscall.o \
		setup64.o e820.o warmreboot.o

ifdef CONFIG_PCI
obj-y			+= pci-x86_64.o
obj-y			+= pci-pc.o pci-irq.o
endif

obj-$(CONFIG_MTRR)	+= mtrr.o
obj-$(CONFIG_X86_MSR)	+= msr.o
obj-$(CONFIG_X86_CPUID)	+= cpuid.o
obj-$(CONFIG_SMP)	+= smp.o smpboot.o trampoline.o
obj-$(CONFIG_X86_LOCAL_APIC)	+= apic.o  nmi.o
obj-$(CONFIG_X86_IO_APIC)	+= io_apic.o mpparse.o
obj-$(CONFIG_EARLY_PRINTK) +=  early_printk.o
obj-$(CONFIG_SWIOTLB) += swiotlb.o
obj-$(CONFIG_GART_IOMMU) += pci-gart.o aperture.o
obj-$(CONFIG_DUMMY_IOMMU) += pci-nommu.o
obj-$(CONFIG_MCE) += bluesmoke.o
obj-$(CONFIG_ACPI_BOOT)		+= acpi.o
obj-$(CONFIG_ACPI_SLEEP)	+= acpi_wakeup.o suspend.o
 

include $(TOPDIR)/Rules.make

