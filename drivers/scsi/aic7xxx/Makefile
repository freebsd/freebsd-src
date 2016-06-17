#
# drivers/scsi/aic7xxx/Makefile
#
# Makefile for the Linux aic7xxx SCSI driver.
#

O_TARGET := aic7xxx_drv.o

list-multi	:= aic7xxx.o aic79xx.o

obj-$(CONFIG_SCSI_AIC7XXX)	+= aic7xxx.o
ifeq ($(CONFIG_PCI),y)
obj-$(CONFIG_SCSI_AIC79XX)	+= aic79xx.o
endif

EXTRA_CFLAGS += -I$(TOPDIR)/drivers/scsi -Werror
#EXTRA_CFLAGS += -g

# Platform Specific Files
obj-aic7xxx = aic7xxx_osm.o aic7xxx_proc.o

# Core Files
obj-aic7xxx += aic7xxx_core.o aic7xxx_93cx6.o
ifeq ($(CONFIG_AIC7XXX_REG_PRETTY_PRINT),y)
obj-aic7xxx += aic7xxx_reg_print.o
endif

#EISA Specific Files
AIC7XXX_EISA_ARCH = $(filter i386 alpha,$(ARCH))
ifneq ($(AIC7XXX_EISA_ARCH),)
obj-aic7xxx += aic7770.o
# Platform Specific EISA Files
obj-aic7xxx += aic7770_osm.o
endif

#PCI Specific Files
ifeq ($(CONFIG_PCI),y)
obj-aic7xxx += aic7xxx_pci.o
# Platform Specific PCI Files
obj-aic7xxx += aic7xxx_osm_pci.o
endif

# Platform Specific U320 Files
obj-aic79xx = aic79xx_osm.o aic79xx_proc.o aic79xx_osm_pci.o
# Core Files
obj-aic79xx += aic79xx_core.o aic79xx_pci.o
ifeq ($(CONFIG_AIC79XX_REG_PRETTY_PRINT),y)
obj-aic79xx += aic79xx_reg_print.o
endif

# Override our module desitnation
MOD_DESTDIR = $(shell cd .. && $(CONFIG_SHELL) $(TOPDIR)/scripts/pathdown.sh)

include $(TOPDIR)/Rules.make

aic7xxx_core.o: aic7xxx_seq.h
$(obj-aic7xxx): aic7xxx_reg.h
aic7xxx.o: aic7xxx_seq.h aic7xxx_reg.h $(obj-aic7xxx)
	$(LD) $(LD_RFLAG) -r -o $@ $(obj-aic7xxx)

aic79xx_core.o: aic79xx_seq.h
$(obj-aic79xx): aic79xx_reg.h
aic79xx.o: aic79xx_seq.h aic79xx_reg.h $(obj-aic79xx)
	$(LD) $(LD_RFLAG) -r -o $@ $(obj-aic79xx)

ifeq ($(CONFIG_AIC7XXX_BUILD_FIRMWARE),y)
aic7xxx_gen = aic7xxx_seq.h aic7xxx_reg.h
ifeq ($(CONFIG_AIC7XXX_REG_PRETTY_PRINT),y)
aic7xxx_gen += aic7xxx_reg_print.c
aic7xxx_asm_cmd = aicasm/aicasm -I. -r aic7xxx_reg.h		\
		 -p aic7xxx_reg_print.c -i aic7xxx_osm.h	\
		 -o aic7xxx_seq.h aic7xxx.seq
else
aic7xxx_asm_cmd = aicasm/aicasm -I. -r aic7xxx_reg.h		\
		 -o aic7xxx_seq.h aic7xxx.seq
endif
$(aic7xxx_gen): aic7xxx.seq aic7xxx.reg aicasm/aicasm
	$(aic7xxx_asm_cmd)
endif

ifeq ($(CONFIG_AIC79XX_BUILD_FIRMWARE),y)
aic79xx_gen = aic79xx_seq.h aic79xx_reg.h
ifeq ($(CONFIG_AIC79XX_REG_PRETTY_PRINT),y)
aic79xx_gen += aic79xx_reg_print.c
aic79xx_asm_cmd = aicasm/aicasm -I. -r aic79xx_reg.h		\
		 -p aic79xx_reg_print.c -i aic79xx_osm.h	\
		 -o aic79xx_seq.h aic79xx.seq
else
aic79xx_asm_cmd = aicasm/aicasm -I. -r aic79xx_reg.h \
		 -o aic79xx_seq.h aic79xx.seq
endif
$(aic79xx_gen): aic79xx.seq aic79xx.reg aicasm/aicasm
	$(aic79xx_asm_cmd)
endif

aicasm/aicasm: aicasm/*.[chyl]
	$(MAKE) -C aicasm
