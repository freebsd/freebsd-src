# $FreeBSD: src/sys/boot/pc98/boot0.5/Makefile,v 1.1.2.1 2000/09/11 09:19:46 kato Exp $

PROG=		boot0.5
OBJS=		start.o boot.o boot0.5.o disk.o selector.o support.o \
		syscons.o
NOMAN=
STRIP=
BINDIR?=	/boot
BINMODE=	444

ORG=		0x000

# The size of boot0.5 must be 7168 bytes
boot0.5: boot0.5.bin
	cat boot0.5.bin /dev/zero | dd of=boot0.5 bs=1 count=7168

boot0.5.bin: boot0.5.out
	objcopy -S -O binary boot0.5.out ${.TARGET}

boot0.5.out: ${OBJS}
	${LD} -N -e start -Ttext ${ORG} -o ${.TARGET} ${OBJS}

CLEANFILES+=	boot0.5.out boot0.5.bin

.include <bsd.prog.mk>
