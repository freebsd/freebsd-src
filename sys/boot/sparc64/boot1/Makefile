# $FreeBSD$

PROG=		boot1
SRCS=		_start.S boot1.c
NOMAN=
STRIP=
BINDIR?=	/boot
BINMODE=	444

BOOTBLOCKBASE=	0x4000

CFLAGS=	-W -Wall -Wno-unused -I../../ -I../../common/ -Os -ffreestanding \
	-mcmodel=medlow

boot1.elf: _start.o boot1.o
	${LD} -N -Ttext ${BOOTBLOCKBASE} -o ${.TARGET} ${.ALLSRC}

boot1.aout: boot1.elf
	elf2aout -o ${.TARGET} ${.ALLSRC}

# Construct boot1. disklabel expects it to contain zeroed-out space for the
# label, and to be of the correct size.
boot1: boot1.aout
	dd if=/dev/zero of=${.TARGET} bs=512 count=16
	dd if=boot1.aout of=${.TARGET} bs=512 oseek=1 conv=notrunc

CLEANFILES+=	boot1.elf boot1.aout

.include <bsd.prog.mk>
