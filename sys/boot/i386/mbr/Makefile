# $FreeBSD$

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
