#
# Makefile for the linux kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#
# Note 2! The CFLAGS definitions are now in the main makefile...

USE_STANDARD_AS_RULE	:= true

ifdef CONFIG_PPC64BRIDGE
EXTRA_AFLAGS		:= -Wa,-mppc64bridge
endif
ifdef CONFIG_40x
EXTRA_AFLAGS		:= -Wa,-m405
endif

# Extra CFLAGS so we don't have to do relative includes
CFLAGS_pmac_setup.o	+= -I$(TOPDIR)/drivers/scsi -I$(TOPDIR)/fs/partitions \
				-I$(TOPDIR)/arch/$(ARCH)/mm
CFLAGS_spruce_setup.o	+= -I$(TOPDIR)/arch/ppc/kernel
CFLAGS_spruce_pci.o	+= -I$(TOPDIR)/arch/ppc/kernel
CFLAGS_pal4_setup.o	+= -I$(TOPDIR)/arch/ppc/kernel
CFLAGS_pal4_pci.o	+= -I$(TOPDIR)/arch/ppc/kernel

O_TARGET := platform.o

export-objs			:= prep_setup.o ibm440gp.o ibm440gx.o

obj-$(CONFIG_405GP)		+= ibm405gp.o
obj-$(CONFIG_440GP)		+= ibm440gp.o
obj-$(CONFIG_440GX)		+= ibm440gx.o

obj-$(CONFIG_CPCI405)		+= cpci405.o
obj-$(CONFIG_EP405)		+= ep405.o
obj-$(CONFIG_REDWOOD_5)		+= redwood5.o ibmstb4.o
obj-$(CONFIG_REDWOOD_6)		+= redwood6.o ibmstbx25.o
obj-$(CONFIG_EBONY)		+= ebony.o
obj-$(CONFIG_OCOTEA)		+= ocotea.o
obj-$(CONFIG_WALNUT)		+= walnut.o

obj-$(CONFIG_APUS)		+= apus_setup.o
ifeq ($(CONFIG_APUS),y)
obj-$(CONFIG_PCI)		+= apus_pci.o
endif

obj-$(CONFIG_ALL_PPC)		+= pmac_pic.o pmac_setup.o pmac_time.o \
					pmac_feature.o pmac_pci.o chrp_setup.o\
					chrp_time.o chrp_pci.o prep_pci.o \
					prep_time.o prep_setup.o pmac_sleep.o \
					pmac_nvram.o
obj-$(CONFIG_PMAC_BACKLIGHT)	+= pmac_backlight.o
obj-$(CONFIG_PPC_RTAS)		+= error_log.o proc_rtas.o
obj-$(CONFIG_PREP_RESIDUAL)	+= residual.o
obj-$(CONFIG_GEMINI)		+= gemini_pci.o gemini_setup.o gemini_prom.o
obj-$(CONFIG_LOPEC)		+= lopec_setup.o lopec_pci.o
obj-$(CONFIG_PAL4)		+= pal4_setup.o pal4_pci.o cpc700_pic.o
obj-$(CONFIG_PPLUS)		+= pplus_pci.o pplus_setup.o
obj-$(CONFIG_PRPMC750)		+= prpmc750.o
obj-$(CONFIG_SANDPOINT)		+= sandpoint.o
obj-$(CONFIG_SPRUCE)		+= spruce_setup.o spruce_pci.o cpc700_pic.o

ifeq ($(CONFIG_SMP),y)
obj-$(CONFIG_ALL_PPC)		+= pmac_smp.o chrp_smp.o
endif

include $(TOPDIR)/Rules.make
