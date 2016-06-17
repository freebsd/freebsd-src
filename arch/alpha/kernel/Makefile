#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

.S.s:
	$(CPP) $(AFLAGS) -o $*.s $<
.S.o:
	$(CC) $(AFLAGS) -c -o $*.o $<

O_TARGET := kernel.o

export-objs := alpha_ksyms.o

obj-y    := entry.o traps.o process.o osf_sys.o irq.o irq_alpha.o \
	    signal.o setup.o ptrace.o time.o semaphore.o alpha_ksyms.o \
	    err_common.o

#
# FIXME!
# These should be made conditional on the stuff that needs them!
#
obj-y	 += irq_i8259.o irq_srm.o

ifdef CONFIG_VGA_HOSE
obj-y	 += console.o
endif

obj-$(CONFIG_SMP)    += smp.o irq_smp.o
obj-$(CONFIG_PCI)    += pci.o pci_iommu.o
obj-$(CONFIG_SRM_ENV)	+= srm_env.o

ifdef CONFIG_ALPHA_GENERIC

obj-y 	 += core_apecs.o core_cia.o core_irongate.o core_lca.o core_mcpcia.o \
	    core_polaris.o core_t2.o core_tsunami.o \
	    sys_alcor.o sys_cabriolet.o sys_dp264.o sys_eb64p.o sys_eiger.o \
	    sys_jensen.o sys_miata.o sys_mikasa.o sys_nautilus.o \
	    sys_noritake.o sys_rawhide.o sys_ruffian.o sys_rx164.o \
	    sys_sable.o sys_sio.o sys_sx164.o sys_takara.o sys_rx164.o \
	    irq_pyxis.o srmcons.o gct.o \
	    es1888.o smc37c669.o smc37c93x.o ns87312.o

ifndef CONFIG_ALPHA_LEGACY_START_ADDRESS
obj-y 	 += core_titan.o core_wildfire.o core_marvel.o \
	    sys_titan.o sys_wildfire.o sys_marvel.o \
	    err_titan.o err_marvel.o

endif

else

# Misc support
obj-$(CONFIG_ALPHA_SRM) += srmcons.o gct.o

# Core logic support
obj-$(CONFIG_ALPHA_APECS) += core_apecs.o
obj-$(CONFIG_ALPHA_CIA) += core_cia.o

obj-$(CONFIG_ALPHA_IRONGATE) += core_irongate.o
obj-$(CONFIG_ALPHA_LCA) += core_lca.o
obj-$(CONFIG_ALPHA_MARVEL) += core_marvel.o
obj-$(CONFIG_ALPHA_MCPCIA) += core_mcpcia.o
obj-$(CONFIG_ALPHA_T2) += core_t2.o
obj-$(CONFIG_ALPHA_TSUNAMI) += core_tsunami.o
obj-$(CONFIG_ALPHA_TITAN) += core_titan.o
obj-$(CONFIG_ALPHA_POLARIS) += core_polaris.o
obj-$(CONFIG_ALPHA_WILDFIRE) += core_wildfire.o

# Board support
ifneq ($(CONFIG_ALPHA_ALCOR)$(CONFIG_ALPHA_XLT),)
obj-y    += sys_alcor.o
endif
ifneq ($(CONFIG_ALPHA_CABRIOLET)$(CONFIG_ALPHA_EB164)$(CONFIG_ALPHA_EB66P)$(CONFIG_ALPHA_LX164)$(CONFIG_ALPHA_PC164),)
obj-y    += sys_cabriolet.o
endif

obj-$(CONFIG_ALPHA_DP264) += sys_dp264.o
obj-$(CONFIG_ALPHA_SHARK) += sys_dp264.o
obj-$(CONFIG_ALPHA_TITAN) += sys_titan.o err_titan.o

ifneq ($(CONFIG_ALPHA_EB64P)$(CONFIG_ALPHA_EB66),)
obj-y    += sys_eb64p.o
endif

obj-$(CONFIG_ALPHA_EIGER) += sys_eiger.o
obj-$(CONFIG_ALPHA_JENSEN) += sys_jensen.o pci-noop.o
obj-$(CONFIG_ALPHA_MARVEL) += sys_marvel.o err_marvel.o
obj-$(CONFIG_ALPHA_MIATA) += sys_miata.o
obj-$(CONFIG_ALPHA_MIKASA) += sys_mikasa.o
obj-$(CONFIG_ALPHA_NAUTILUS) += sys_nautilus.o
obj-$(CONFIG_ALPHA_NORITAKE) += sys_noritake.o
obj-$(CONFIG_ALPHA_RAWHIDE) += sys_rawhide.o
obj-$(CONFIG_ALPHA_RUFFIAN) += sys_ruffian.o
obj-$(CONFIG_ALPHA_RX164) += sys_rx164.o

ifneq ($(CONFIG_ALPHA_SABLE)$(CONFIG_ALPHA_LYNX),)
obj-y    += sys_sable.o
endif

ifneq ($(CONFIG_ALPHA_BOOK1)$(CONFIG_ALPHA_AVANTI)$(CONFIG_ALPHA_NONAME)$(CONFIG_ALPHA_P2K)$(CONFIG_ALPHA_XL),)
obj-y    += sys_sio.o
endif

obj-$(CONFIG_ALPHA_SX164) += sys_sx164.o
obj-$(CONFIG_ALPHA_TAKARA) += sys_takara.o
obj-$(CONFIG_ALPHA_WILDFIRE) += sys_wildfire.o

ifneq ($(CONFIG_ALPHA_MIATA)$(CONFIG_ALPHA_RUFFIAN)$(CONFIG_ALPHA_SX164),)
obj-y    += irq_pyxis.o
endif

ifneq ($(CONFIG_ALPHA_MIATA)$(CONFIG_ALPHA_DP264),)
obj-y    += es1888.o
endif

ifneq ($(CONFIG_ALPHA_DP264)$(CONFIG_ALPHA_MIATA)$(CONFIG_ALPHA_SX164)$(CONFIG_ALPHA_TITAN),)
obj-y    += smc37c669.o
endif

ifneq ($(CONFIG_ALPHA_PC164)$(CONFIG_ALPHA_LX164),)
obj-y    += smc37c93x.o
endif

ifneq ($(CONFIG_ALPHA_NONAME)$(CONFIG_ALPHA_EB164)$(CONFIG_ALPHA_AVANTI)$(CONFIG_ALPHA_P2K)$(CONFIG_ALPHA_XL)$(CONFIG_ALPHA_TAKARA),)
obj-y    += ns87312.o
endif

endif # GENERIC

all: kernel.o head.o

asm_offsets: check_asm
	./check_asm > $(TOPDIR)/include/asm-alpha/asm_offsets.h

check_asm: check_asm.c
	$(HOSTCC) -o $@ $< $(CPPFLAGS) -ffixed-8

clean::
	rm -f check_asm

include $(TOPDIR)/Rules.make
