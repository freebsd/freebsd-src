# $FreeBSD: src/sys/boot/i386/mbr/Makefile,v 1.2 1999/08/28 00:40:20 peter Exp $

PROG=		mbr
NOMAN=
STRIP=
BINDIR?=	/boot
BINMODE=	444

M4?=	m4

ORG=	0x600

mbr: mbr.o
.if ${OBJFORMAT} == aout
	${LD} -N -s -T ${ORG} -o mbr.out mbr.o
	dd if=mbr.out of=${.TARGET} ibs=32 skip=1
.else
	${LD} -N -e start -Ttext ${ORG} -o mbr.out mbr.o
	objcopy -S -O binary mbr.out ${.TARGET}
.endif

mbr.o: mbr.m4 mbr.s
	(cd ${.CURDIR}; ${M4} mbr.m4 mbr.s) | \
		${AS} ${AFLAGS} -o ${.TARGET}

CLEANFILES+= mbr.out mbr.o

.include <bsd.prog.mk>
