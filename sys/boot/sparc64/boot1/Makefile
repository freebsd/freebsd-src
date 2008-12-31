# $FreeBSD: src/sys/boot/sparc64/boot1/Makefile,v 1.11.18.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	boot1.elf
INTERNALPROG=
NO_MAN=
FILES=	boot1
SRCS=	_start.s boot1.c

BOOTBLOCKBASE= 0x4000

CFLAGS=	-mcmodel=medlow -Os -I${.CURDIR}/../../common
LDFLAGS=-N -Ttext ${BOOTBLOCKBASE}

# Construct boot1. sunlabel expects it to contain zeroed-out space for the
# label, and to be of the correct size.
boot1: boot1.aout
	dd if=/dev/zero of=${.TARGET} bs=512 count=16
	dd if=boot1.aout of=${.TARGET} bs=512 oseek=1 conv=notrunc

CLEANFILES= boot1.aout

boot1.aout: boot1.elf
	elf2aout -o ${.TARGET} ${.ALLSRC}

boot1.o: ${.CURDIR}/../../common/ufsread.c

.include <bsd.prog.mk>
