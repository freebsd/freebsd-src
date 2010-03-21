#	pccardc Makefile
#
# $FreeBSD: src/usr.sbin/pccard/dumpcis/Makefile,v 1.1.10.1 2010/02/10 00:26:20 kensmith Exp $

.PATH: ${.CURDIR}/../pccardd ${.CURDIR}/../pccardc

PROG=	dumpcis
MAN=	dumpcis.8
SRCS=	main.c dumpcis.c dumpcisfile.c readcis.c printcis.c

CFLAGS+= -I${.CURDIR}/../pccardd -I${.CURDIR}/../pccardc

.include <bsd.prog.mk>
