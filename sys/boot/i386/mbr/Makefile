# $FreeBSD: src/sys/boot/i386/mbr/Makefile,v 1.2.2.1 2000/07/06 23:04:29 obrien Exp $

PROG=		mbr
NOMAN=
STRIP=
BINDIR?=	/boot
BINMODE=	444

ORG=	0x600

mbr: mbr.o
.if ${OBJFORMAT} == aout
	${LD} -N -s -T ${ORG} -o mbr.out mbr.o
	dd if=mbr.out of=${.TARGET} ibs=32 skip=1
.else
	${LD} -N -e start -Ttext ${ORG} -o mbr.out mbr.o
	objcopy -S -O binary mbr.out ${.TARGET}
.endif

CLEANFILES+= mbr.out mbr.o

.include <bsd.prog.mk>
