#
# Makefile for the Linux/SuperH kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

all: kernel.o head.o init_task.o

clean:

O_TARGET := kernel.o

export-objs	:= io.o io_generic.o io_hd64461.o setup_hd64461.o sh_ksyms.o \
		   dma.o

obj-y	:= process.o signal.o entry.o traps.o irq.o irq_ipr.o \
	ptrace.o setup.o time.o sys_sh.o semaphore.o \
	irq_imask.o io.o io_generic.o sh_ksyms.o ubc.o

obj-$(CONFIG_CF_ENABLER)	+= cf-enabler.o
obj-$(CONFIG_CPU_SH4)		+= fpu.o
obj-$(CONFIG_SH_RTC)            += rtc.o
obj-$(CONFIG_SH_DMA)		+= dma.o
obj-$(CONFIG_SH_STANDARD_BIOS)	+= sh_bios.o
obj-$(CONFIG_SH_KGDB)		+= kgdb_stub.o kgdb_jmp.o

ifeq ($(CONFIG_PCI),y)
ifeq ($(CONFIG_SH_DREAMCAST),y)
obj-y 				+= pci-dc.o pcibios.o
else
obj-y                           += pci-dma.o pcibios.o
obj-$(CONFIG_CPU_SUBTYPE_ST40)+= pci_st40.o
obj-$(CONFIG_CPU_SUBTYPE_SH7751)+= pci-sh7751.o 
obj-$(CONFIG_SH_BIGSUR)+= pci-bigsur.o
obj-$(CONFIG_SH_7751_SOLUTION_ENGINE)+= pci-7751se.o
obj-$(CONFIG_PCI_SD0001)+= pci-sd0001.o
obj-$(CONFIG_SH_SECUREEDGE5410)	+= pci-snapgear.o
obj-$(CONFIG_SH_KEYWEST)	+= pci-v320usc.o
endif
endif

obj-$(CONFIG_SH_HP600)		+= mach_hp600.o
machine-specific-objs		+= mach_hp600.o

obj-$(CONFIG_SH_HS7729PCI)	+= mach_hs7729pci.o setup_hs7729pci.o io_hs7729pci.o
machine-specific-objs		+= mach_hs7729pci.o setup_hs7729pci.o io_hs7729pci.o

obj-$(CONFIG_SH_SOLUTION_ENGINE)+= mach_se.o setup_se.o io_se.o led_se.o
machine-specific-objs		+= mach_se.o setup_se.o io_se.o led_se.o

obj-$(CONFIG_SH_7751_SOLUTION_ENGINE)+= mach_7751se.o setup_7751se.o \
					io_7751se.o led_se.o
machine-specific-objs           += mach_7751se.o 7751setup_se.o \
				   io_7751se.o led_se.o pci-7751se.o

obj-$(CONFIG_SH_MOBILE_SOLUTION_ENGINE)+= mach_shmse.o setup_shmse.o \
					io_shmse.o led_se.o
machine-specific-objs          	+= mach_shmse.o setup_shmse.o \
				   io_shmse.o led_se.o

obj-$(CONFIG_SH_BIGSUR)		+= mach_bigsur.o setup_bigsur.o io_bigsur.o led_bigsur.o
machine-specific-objs		+= mach_bigsur.o setup_bigsur.o io_bigsur.o led_bigsur.o

obj-$(CONFIG_SH_SH2000)		+= setup_sh2000.o io_sh2000.o
machine-specific-objs		+= setup_sh2000.o io_sh2000.o

obj-$(CONFIG_SH_CAT68701) 	+= mach_cat68701.o io_cat68701.o
machine-specific-objs		+= mach_cat68701.o io_cat68701.o

obj-$(CONFIG_SH_CQREEK)		+= setup_cqreek.o
machine-specific-objs		+= setup_cqreek.o

obj-$(CONFIG_SH_UNKNOWN)	+= mach_unknown.o io_unknown.o
machine-specific-objs		+= mach_unknown.o io_unknown.o

obj-$(CONFIG_HD64461)		+= setup_hd64461.o io_hd64461.o
machine-specific-objs		+= setup_hd64461.o io_hd64461.o

obj-$(CONFIG_CPU_SUBTYPE_ST40) +=irq_intc2.o

obj-$(CONFIG_SH_ADX)		+= mach_adx.o setup_adx.o io_adx.o irq_maskreg.o
machine-specific-objs		+= mach_adx.o setup_adx.o io_adx.o irq_maskreg.o

obj-$(CONFIG_SH_SECUREEDGE5410)	+= mach_snapgear.o setup_snapgear.o io_snapgear.o
machine-specific-objs		+= mach_snapgear.o setup_snapgear.o io_snapgear.o
ifeq ($(CONFIG_RTC),y)
obj-$(CONFIG_SH_SECUREEDGE5410)	+= rtc-snapgear.o
machine-specific-objs		+= rtc-snapgear.o
endif

obj-$(CONFIG_SH_KEYWEST)	+= mach_keywest.o setup_keywest.o io_keywest.o
machine-specific-objs		+= mach_keywest.o setup_keywest.o io_keywest.o

obj-$(CONFIG_SH_SH4202_MICRODEV)+= mach_microdev.o setup_microdev.o io_microdev.o irq_microdev.o
machine-specific-objs		+= mach_microdev.o setup_microdev.o io_microdev.o irq_microdev.o

# Doesn't compile well, so don't include in machine-specific-objs
obj-$(CONFIG_HD64465)		+= setup_hd64465.o io_hd64465.o hd64465_gpio.o
obj-$(CONFIG_SH_DMIDA)		+= mach_dmida.o
obj-$(CONFIG_SH_EC3104)         += setup_ec3104.o io_ec3104.o mach_ec3104.o
obj-$(CONFIG_SH_DREAMCAST)	+= mach_dc.o setup_dc.o io_dc.o rtc-aica.o

ifeq ($(CONFIG_SH_GENERIC),y)
obj-y		+= $(machine-specific-objs)
endif

USE_STANDARD_AS_RULE := true

include $(TOPDIR)/Rules.make
