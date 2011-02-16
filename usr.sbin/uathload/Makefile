# $FreeBSD: src/usr.sbin/uathload/Makefile,v 1.2.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	uathload
MAN=	uathload.8

SRCS=	uathload.c ar5523.bin

WARNS?= 4

CLEANFILES=	ar5523.bin

ar5523.bin: ${.CURDIR}/../../sys/contrib/dev/uath/ar5523.bin.uu
	uudecode -p ${.CURDIR}/../../sys/contrib/dev/uath/ar5523.bin.uu > ${.TARGET}

ar5523.o: ar5523.bin
	${LD} -b binary -d -warn-common -r -d -o ${.TARGET} ar5523.bin

.include <bsd.prog.mk>
