# $FreeBSD: src/sys/boot/i386/mbr/Makefile,v 1.6 2002/09/17 01:48:55 peter Exp $

PROG=		mbr
NOMAN=
STRIP=
BINDIR?=	/boot
BINMODE=	444

ORG=	0x600

mbr: mbr.o
	${LD} -N -e start -Ttext ${ORG} -o mbr.out mbr.o
	objcopy -S -O binary mbr.out ${.TARGET}

CLEANFILES+= mbr.out mbr.o

.include <bsd.prog.mk>
