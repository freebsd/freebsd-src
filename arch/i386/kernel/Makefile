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

all: kernel.o head.o init_task.o

O_TARGET := kernel.o

export-objs     := mca.o mtrr.o msr.o cpuid.o microcode.o i386_ksyms.o time.o setup.o

obj-y	:= process.o semaphore.o signal.o entry.o traps.o irq.o vm86.o \
		ptrace.o i8259.o ioport.o ldt.o setup.o time.o sys_i386.o \
		pci-dma.o i386_ksyms.o i387.o bluesmoke.o dmi_scan.o


ifdef CONFIG_PCI
obj-y			+= pci-i386.o
ifdef CONFIG_VISWS
obj-y			+= pci-visws.o
else
obj-y			+= pci-pc.o pci-irq.o
endif
endif

obj-$(CONFIG_MCA)		+= mca.o
obj-$(CONFIG_MTRR)		+= mtrr.o
obj-$(CONFIG_X86_MSR)		+= msr.o
obj-$(CONFIG_X86_CPUID)		+= cpuid.o
obj-$(CONFIG_MICROCODE)		+= microcode.o
obj-$(CONFIG_APM)		+= apm.o
obj-$(CONFIG_ACPI_BOOT)		+= acpi.o
obj-$(CONFIG_ACPI_SLEEP)	+= acpi_wakeup.o
obj-$(CONFIG_SMP)		+= smp.o smpboot.o trampoline.o
obj-$(CONFIG_X86_LOCAL_APIC)	+= mpparse.o apic.o nmi.o
obj-$(CONFIG_X86_IO_APIC)	+= io_apic.o
obj-$(CONFIG_X86_VISWS_APIC)	+= visws_apic.o
obj-$(CONFIG_EDD)             	+= edd.o

include $(TOPDIR)/Rules.make
