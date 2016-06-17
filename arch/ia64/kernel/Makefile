#
# Makefile for the linux kernel.
#

.S.s:
	$(CPP) $(AFLAGS) $(AFLAGS_KERNEL) -o $*.s $<
.S.o:
	$(CC) $(AFLAGS) $(AFLAGS_KERNEL) -c -o $*.o $<

all: kernel.o head.o init_task.o

O_TARGET := kernel.o

export-objs := ia64_ksyms.o unwind.o

obj-y := acpi.o entry.o gate.o efi.o efi_stub.o ia64_ksyms.o irq.o irq_ia64.o irq_lsapic.o ivt.o \
	 machvec.o pal.o process.o perfmon.o ptrace.o sal.o salinfo.o semaphore.o setup.o	 \
	 signal.o sys_ia64.o traps.o time.o unaligned.o unwind.o
obj-$(CONFIG_IA64_GENERIC) += iosapic.o
obj-$(CONFIG_IA64_SGI_SN2) += iosapic.o
obj-$(CONFIG_IA64_HP_ZX1) += iosapic.o
obj-$(CONFIG_IA64_DIG) += iosapic.o
obj-$(CONFIG_IA64_PALINFO) += palinfo.o
obj-$(CONFIG_EFI_VARS) += efivars.o
obj-$(CONFIG_PCI) += pci.o
obj-$(CONFIG_SMP) += smp.o smpboot.o
obj-$(CONFIG_IA64_MCA) += mca.o mca_asm.o
obj-$(CONFIG_IA64_BRL_EMU) += brl_emu.o

include $(TOPDIR)/Rules.make
