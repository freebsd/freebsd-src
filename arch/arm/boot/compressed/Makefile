#
# linux/arch/arm/boot/compressed/Makefile
#
# create a compressed vmlinuz image from the original vmlinux
#
# Note! SYSTEM, ZTEXTADDR, ZBSSADDR and ZRELADDR are now exported
# from arch/arm/boot/Makefile
#

HEAD		 = head.o
OBJS		 = misc.o
CFLAGS		 = $(CPPFLAGS) -O2 -DSTDC_HEADERS $(CFLAGS_BOOT) -fpic -Uarm
FONTC		 = $(TOPDIR)/drivers/video/font_acorn_8x8.c
ZLDFLAGS	 = -p -X -T vmlinux.lds

#
# Architecture dependencies
#
ifeq ($(CONFIG_ARCH_ACORN),y)
OBJS		+= ll_char_wr.o font.o
CFLAGS		+= -DPARAMS_PHYS=$(PARAMS_PHYS)
endif

ifeq ($(CONFIG_ARCH_RISCSTATION),y)
OBJS		+= ll_char_wr.o font.o
CFLAGS		+= -DPARAMS_PHYS=$(PARAMS_PHYS)
endif

ifeq ($(CONFIG_ARCH_NETWINDER),y)
OBJS		+= head-netwinder.o
endif

ifeq ($(CONFIG_ARCH_SHARK),y)
OBJS		+= head-shark.o ofw-shark.o
endif

ifeq ($(CONFIG_ARCH_INTEGRATOR),y)
OBJS		+= head-integrator.o
endif

ifeq ($(CONFIG_ARCH_AT91RM9200),y)
OBJS		+= head-at91rm9200.o
endif

ifeq ($(CONFIG_ARCH_MX1ADS),y)
OBJS		+= head-mx1ads.o
endif

ifeq ($(CONFIG_ARCH_CAMELOT),y)
OBJS		+= head-epxa10db.o
endif

ifeq ($(CONFIG_ARCH_FTVPCI),y)
OBJS		+= head-ftvpci.o
endif

ifeq ($(CONFIG_ARCH_L7200),y)
OBJS		+= head-l7200.o
endif

ifeq ($(CONFIG_ARCH_CLPS7500),y)
HEAD		= head-clps7500.o
endif

ifeq ($(CONFIG_ARCH_P720T),y)
# Borrow this code from SA1100
OBJS		+= head-sa1100.o
endif

ifeq ($(CONFIG_ARCH_SA1100),y)
OBJS		+= head-sa1100.o
endif

SEDFLAGS	= s/TEXT_START/$(ZTEXTADDR)/;s/LOAD_ADDR/$(ZRELADDR)/;s/BSS_START/$(ZBSSADDR)/

LIBGCC		:= $(shell $(CC) $(CFLAGS) --print-libgcc-file-name)

all:		vmlinux

vmlinux:	$(HEAD) $(OBJS) piggy.o vmlinux.lds
		$(LD) $(ZLDFLAGS) $(HEAD) $(OBJS) piggy.o $(LIBGCC) -o vmlinux

$(HEAD): 	$(HEAD:.o=.S) \
			$(wildcard $(TOPDIR)/include/config/zboot/rom.h) \
			$(wildcard $(TOPDIR)/include/config/cpu/32.h) \
			$(wildcard $(TOPDIR)/include/config/cpu/26.h)
		$(CC) $(AFLAGS) -traditional -c $(HEAD:.o=.S)

piggy.o:	$(SYSTEM)
		$(OBJCOPY) $(OBJCOPYFLAGS) $(SYSTEM) piggy
		gzip $(GZFLAGS) < piggy > piggy.gz
		$(LD) -r -o $@ -b binary piggy.gz
		rm -f piggy piggy.gz

font.o:		$(FONTC)
		$(CC) $(CFLAGS) -Dstatic= -c -o $@ $(FONTC)

vmlinux.lds:	vmlinux.lds.in Makefile $(TOPDIR)/arch/$(ARCH)/boot/Makefile $(TOPDIR)/.config
		@sed "$(SEDFLAGS)" < vmlinux.lds.in > $@

clean:;		rm -f vmlinux core piggy* vmlinux.lds

.PHONY:	clean

misc.o: misc.c $(TOPDIR)/include/asm/arch/uncompress.h $(TOPDIR)/lib/inflate.c

%.o: %.S
	$(CC) $(AFLAGS) $(EXTRA_AFLAGS) $(AFLAGS_$@) -c -o $@ $<
