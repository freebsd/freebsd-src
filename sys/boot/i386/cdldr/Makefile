# $FreeBSD: src/sys/boot/i386/cdldr/Makefile,v 1.1 2000/01/27 21:21:00 jhb Exp $

MAINTAINER=jhb@FreeBSD.org

ORG=0x00

all: cdldr

cdldr: cdldr.o
.if ${OBJFORMAT} == aout
	${LD} -nostdlib -N -s -T ${ORG} -o cdldr.out cdldr.o
	dd if=cdldr.out of=${.TARGET} ibs=32 skip=1
.else
	${LD} -N -e start -Ttext ${ORG} -o cdldr.out cdldr.o
	objcopy -S -O binary cdldr.out ${.TARGET}
.endif

cdldr.o: cdldr.s
	${AS} ${AFLAGS} -o ${.TARGET} ${.CURDIR}/cdldr.s

CLEANFILES+= cdldr cdldr.out cdldr.o

.include <bsd.prog.mk>
