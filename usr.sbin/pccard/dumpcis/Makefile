#	pccardc Makefile
#
# $FreeBSD: src/usr.sbin/pccard/dumpcis/Makefile,v 1.1 2006/10/18 21:09:43 imp Exp $

.PATH: ${.CURDIR}/../pccardd ${.CURDIR}/../pccardc

PROG=	dumpcis
MAN=	dumpcis.8
SRCS=	main.c dumpcis.c dumpcisfile.c readcis.c printcis.c

CFLAGS+= -I${.CURDIR}/../pccardd -I${.CURDIR}/../pccardc

.include <bsd.prog.mk>
