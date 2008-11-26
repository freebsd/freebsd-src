#	pccardc Makefile
#
# $FreeBSD: src/usr.sbin/pccard/pccardc/Makefile,v 1.16.28.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../pccardd

PROG=	pccardc
MAN=	pccardc.8
SRCS=	beep.c dumpcis.c dumpcisfile.c enabler.c pccardc.c pccardmem.c power.c \
	printcis.c rdattr.c rdmap.c rdreg.c readcis.c wrattr.c wrreg.c

CFLAGS+= -I${.CURDIR}/../pccardd

.include <bsd.prog.mk>
