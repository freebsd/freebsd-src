# $FreeBSD$

MAINTAINER=	rnordier

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
