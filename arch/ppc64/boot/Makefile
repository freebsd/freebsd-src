# Makefile for making ELF bootable images for booting on CHRP
# using Open Firmware.
#
# Geert Uytterhoeven	September 1997
#
# Based on coffboot by Paul Mackerras
# Simplified for ppc64 by Todd Inglett
#
# NOTE:	this code is built for 32 bit in ELF32 format even though
#	it packages a 64 bit kernel.  We do this to simplify the
#	bootloader and increase compatibility with OpenFirmware.
#
#	To this end we need to define BOOTCC, etc, as the tools
#	needed to build the 32 bit image.  These are normally HOSTCC,
#	but may be a third compiler if, for example, you are cross
#	compiling from an intel box.  Once the 64bit ppc gcc is
#	stable it will probably simply be a compiler switch to
#	compile for 32bit mode.
#	To make it easier to setup a cross compiler,
#	CROSS32_COMPILE is setup as a prefix just like CROSS_COMPILE
#	in the toplevel makefile.

CROSS32_COMPILE =
#CROSS32_COMPILE = /usr/local/ppc/bin/powerpc-linux-

BOOTCC = $(CROSS32_COMPILE)gcc
BOOTCFLAGS = $(HOSTCFLAGS) -fno-builtin -I$(HPATH)
BOOTLD = $(CROSS32_COMPILE)ld
BOOTAS = $(CROSS32_COMPILE)as
BOOTAFLAGS = -D__ASSEMBLY__ $(HOSTCFLAGS) -I$(HPATH)

OBJCOPYFLAGS = contents,alloc,load,readonly,data

LDVARS=	\
	--defsym _vmlinux_memsize=0x`$(CROSS_COMPILE)nm -n $(TOPDIR)/vmlinux | sed '$$s/^........\([^ ]*\).*/\1/p;d'` \
	--defsym _vmlinux_filesize=0x`ls -l $(TOPDIR)/vmlinux | awk '{ printf "%x\n", $$5 }'`

.c.o:
	$(BOOTCC) $(BOOTCFLAGS) -c -o $*.o $<
.S.o:
	$(BOOTCC) $(BOOTAFLAGS) -traditional -c -o $*.o $<

CFLAGS	= $(CPPFLAGS) -O -fno-builtin -DSTDC_HEADERS
LD_ARGS = -Ttext 0x00400000 -e _start

OBJS = crt0.o string.o prom.o zImage.o zlib.o
LIBS =

ifeq ($(CONFIG_SMP),y)
TFTPIMAGE=/tftpboot/zImage.chrp.smp
else
TFTPIMAGE=/tftpboot/zImage.chrp
endif


ifeq ($(CONFIG_PPC_ISERIES),y)
all:	vmlinux.sm
else
all:	zImage
endif


znetboot: zImage
	cp zImage $(TFTPIMAGE)


ifeq ($(CONFIG_PPC_ISERIES),y)

addSystemMap: addSystemMap.c
	$(HOSTCC) $(HOSTCFLAGS) -o addSystemMap addSystemMap.c

vmlinux.sm: $(TOPDIR)/vmlinux addSystemMap
	./addSystemMap $(TOPDIR)/System.map $(TOPDIR)/vmlinux vmlinux.sm


addRamDisk: addRamDisk.c
	$(HOSTCC) $(HOSTCFLAGS) -o addRamDisk addRamDisk.c

vmlinux.initrd: $(TOPDIR)/vmlinux addRamDisk ramdisk.image.gz $(TOPDIR)/System.map
	./addRamDisk ramdisk.image.gz $(TOPDIR)/System.map $(TOPDIR)/vmlinux vmlinux.initrd

vmlinux.sminitrd: vmlinux.sm addRamDisk ramdisk.image.gz $(TOPDIR)/System.map
	./addRamDisk ramdisk.image.gz $(TOPDIR)/System.map vmlinux.sm vmlinux.sminitrd

endif


znetboot.initrd: zImage.initrd
	cp zImage.initrd $(TFTPIMAGE)

addnote: addnote.c
	$(HOSTCC) $(HOSTCFLAGS) -o addnote addnote.c


zImage.o: $(TOPDIR)/vmlinux

uts_string: uts_string.txt
	$(OBJCOPY) zImage.o \
		--add-section=.kernel:$@=$@.txt \
		--set-section-flags=.kernel:$@=$(OBJCOPYFLAGS)

vmlinux .config System.map: % : $(TOPDIR)/% zImage.o
	gzip -cvf9 $(TOPDIR)/$@ > kernel-$@.gz
	$(OBJCOPY) zImage.o \
		--add-section=.kernel:$@=kernel-$@.gz \
		--set-section-flags=.kernel:$@=$(OBJCOPYFLAGS)

initrd: ramdisk.image.gz
	$(OBJCOPY) zImage.o \
		--add-section=.kernel:$@=ramdisk.image.gz \
		--set-section-flags=.kernel:$@=$(OBJCOPYFLAGS)

uts_string.txt: $(TOPDIR)/vmlinux
	strings $(TOPDIR)/vmlinux | \
	grep -E 'Linux version .* .gcc version' > $@

zImage: $(OBJS) addnote uts_string vmlinux .config System.map
	$(BOOTLD) $(LD_ARGS) -T zImage.lds -o $@ $(OBJS) $(LIBS) \
        $(LDVARS)
	./addnote $@

zImage.initrd: $(OBJS) addnote uts_string vmlinux .config System.map initrd
	$(BOOTLD) $(LD_ARGS) -T zImage.lds -o $@ $(OBJS) $(LIBS) \
        $(LDVARS)
	./addnote $@

clean:
	rm -f add{note,RamDisk,SystemMap} $(OBJS) initrd.o \
		vmlinux.{sm,initrd} zImage{,.initrd} uts_string.txt \
		kernel-{vmlinux,.config,System.map}.gz

fastdep:
	$(TOPDIR)/scripts/mkdep *.[Sch] > .depend

dep:
	$(CPP) $(CPPFLAGS) -M *.S *.c > .depend

