#
# arch/arm/boot/Makefile
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 1995-2002 Russell King
#

SYSTEM	=$(TOPDIR)/vmlinux

# Note: the following conditions must always be true:
#  ZRELADDR == virt_to_phys(TEXTADDR)
#   PARAMS_PHYS must be with 4MB of ZRELADDR
#   INITRD_PHYS must be in RAM

ifeq ($(CONFIG_CPU_26),y)
ZRELADDR	 = 0x02080000
PARAMS_PHYS	 = 0x0207c000
INITRD_PHYS	 = 0x02180000
endif

ifeq ($(CONFIG_ARCH_RPC),y)
ZRELADDR	 = 0x10008000
PARAMS_PHYS	 = 0x10000100
INITRD_PHYS	 = 0x18000000
endif

ifeq ($(CONFIG_ARCH_RISCSTATION),y)
ZRELADDR	 = 0x10008000
PARAMS_PHYS	 = 0x10000100
INITRD_PHYS	 = 0x18000000
endif

ifeq ($(CONFIG_ARCH_CLPS7500),y)
ZRELADDR	 = 0x10008000
endif

ifeq ($(CONFIG_ARCH_EBSA110),y)
ZRELADDR	 = 0x00008000
PARAMS_PHYS	 = 0x00000400
INITRD_PHYS	 = 0x00800000
endif

ifeq ($(CONFIG_ARCH_SHARK),y)
ZTEXTADDR	 = 0x08508000
ZRELADDR	 = 0x08008000
endif

ifeq ($(CONFIG_FOOTBRIDGE),y)
ZRELADDR	 = 0x00008000
PARAMS_PHYS	 = 0x00000100
INITRD_PHYS	 = 0x00800000
endif

ifeq ($(CONFIG_ARCH_INTEGRATOR),y)
ZRELADDR	 = 0x00008000
PARAMS_PHYS	 = 0x00000100
INITRD_PHYS	 = 0x00800000
endif

ifeq ($(CONFIG_ARCH_AT91RM9200),y)
ZRELADDR	 = 0x20008000
endif

ifeq ($(CONFIG_ARCH_MX1ADS),y)
ZRELADDR	 = 0x08008000
endif

ifeq ($(CONFIG_ARCH_CAMELOT),y)
ZRELADDR	 = 0x00008000
endif

ifeq ($(CONFIG_ARCH_NEXUSPCI),y)
ZRELADDR	 = 0x40008000
endif

ifeq ($(CONFIG_ARCH_L7200),y)
ZRELADDR	 = 0xf0008000
endif

# The standard locations for stuff on CLPS711x type processors
ifeq ($(CONFIG_ARCH_CLPS711X),y)
ZRELADDR	 = 0xc0028000
PARAMS_PHYS	 = 0xc0000100
endif

# Should probably have some agreement on these...
ifeq ($(CONFIG_ARCH_P720T),y)
INITRD_PHYS	 = 0xc0400000
endif
ifeq ($(CONFIG_ARCH_CDB89712),y)
INITRD_PHYS	 = 0x00700000
endif

ifeq ($(CONFIG_ARCH_OMAHA),y)
ZTEXTADDR	 = 0x0c008000
PARAMS_PHYS	 = 0x0C000100
INITRD_PHYS	 = 0x0C800000
INITRD_VIRT	 = 0x0C800000
endif

ifeq ($(CONFIG_ARCH_SA1100),y)
ZRELADDR	 = 0xc0008000
# No defconfig file to move this into...
#ifeq ($(CONFIG_SA1100_YOPY),y)
#  ZTEXTADDR      = 0x00080000
#  ZBSSADDR       = 0xc0200000
#endif
ifeq ($(CONFIG_SA1111),y)
  ZRELADDR	 = 0xc0208000
endif
endif

ifeq ($(CONFIG_ARCH_ANAKIN),y)
ZRELADDR	 = 0x20008000
endif

#
# We now have a PIC decompressor implementation.  Decompressors running
# from RAM should not define ZTEXTADDR.  Decompressors running directly
# from ROM or Flash must define ZTEXTADDR (preferably via the config)
#
ifeq ($(CONFIG_ZBOOT_ROM),y)
ZTEXTADDR	=0x$(CONFIG_ZBOOT_ROM_TEXT)
ZBSSADDR	=0x$(CONFIG_ZBOOT_ROM_BSS)
else
ZTEXTADDR	=0
ZBSSADDR	=ALIGN(4)
endif

export	SYSTEM ZTEXTADDR ZBSSADDR ZRELADDR INITRD_PHYS PARAMS_PHYS

Image:	$(SYSTEM)
	$(OBJCOPY) $(OBJCOPYFLAGS) $< $@

bzImage: zImage

zImage:	compressed/vmlinux
	$(OBJCOPY) $(OBJCOPYFLAGS) $< $@

bootpImage: bootp/bootp
	$(OBJCOPY) $(OBJCOPYFLAGS) $< $@

compressed/vmlinux: $(TOPDIR)/vmlinux FORCE
	@$(MAKE) -C compressed vmlinux

bootp/bootp: zImage initrd FORCE
	@$(MAKE) -C bootp bootp

initrd:
	@test "$(INITRD_PHYS)" != "" || (echo This machine does not support INITRD; exit -1)
	@test "$(INITRD)" != "" || (echo You must specify INITRD; exit -1)

install: Image
	sh ./install.sh $(VERSION).$(PATCHLEVEL).$(SUBLEVEL)$(EXTRAVERSION) Image $(TOPDIR)/System.map "$(INSTALL_PATH)"

zinstall: zImage
	sh ./install.sh $(VERSION).$(PATCHLEVEL).$(SUBLEVEL)$(EXTRAVERSION) zImage $(TOPDIR)/System.map "$(INSTALL_PATH)"

clean:
	$(RM) Image zImage bootpImage
	@$(MAKE) -C compressed clean
	@$(MAKE) -C bootp clean

dep:

FORCE:
