
.include <bsd.init.mk>

PROG=           boot1.elf
NEWVERSWHAT=    "Open Firmware boot block" ${MACHINE_ARCH}
INSTALLFLAGS=   -b

FILES=		boot1.hfs
SRCS=		boot1.c ashldi3.c syncicache.c

CFLAGS+=-I${LDRSRC}
# Load boot1.elf below kernel.
#
# Due to limitiations in the way we load stuff, we have to avoid reusing
# memory until the kernel MMU code has taken over.
#
# 0x38000 is high enough to not interfere with the trap area, but low
# enough that it doesn't bump into the kernel area starting at 0x100000,
# even if the entire partition gets used as the load size by a buggy OFW.
#
# In theory 0xf0000 would work too under the current 64k size limit for
# boot1.elf defined in the HFS template, but sometimes boot1.elf is written
# directly to the PReP partition.
#
LDFLAGS=-nostdlib -static -Wl,-N -Wl,-Ttext=0x38000

.PATH:  ${SYSDIR}/libkern ${SRCTOP}/lib/libc/powerpc/gen ${.CURDIR}

# The following inserts out objects into a template HFS 
# created by generate-hfs.sh

.include "${.CURDIR}/Makefile.hfs"

boot1.hfs: boot1.elf bootinfo.txt
	echo ${.OBJDIR}
	uudecode ${.CURDIR}/hfs.tmpl.bz2.uu
	mv hfs.tmpl.bz2 ${.TARGET}.bz2
	bzip2 -f -d ${.TARGET}.bz2
	${DD} if=boot1.elf of=${.TARGET} seek=${BOOT1_OFFSET} conv=notrunc
	${DD} if=${.CURDIR}/bootinfo.txt of=${.TARGET} seek=${BOOTINFO_OFFSET} \
	    conv=notrunc

CLEANFILES+= boot1.hfs

MK_PIE=		no

.include <bsd.prog.mk>

