#	pccardc Makefile
#
# $FreeBSD: src/usr.sbin/pccard/pccardc/Makefile,v 1.16.36.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../pccardd

PROG=	pccardc
MAN=	pccardc.8
SRCS=	beep.c dumpcis.c dumpcisfile.c enabler.c pccardc.c pccardmem.c power.c \
	printcis.c rdattr.c rdmap.c rdreg.c readcis.c wrattr.c wrreg.c

CFLAGS+= -I${.CURDIR}/../pccardd

.include <bsd.prog.mk>
