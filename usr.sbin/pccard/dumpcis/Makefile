#	pccardc Makefile
#
# $FreeBSD: src/usr.sbin/pccard/dumpcis/Makefile,v 1.1.12.1 2010/12/21 17:10:29 kensmith Exp $

.PATH: ${.CURDIR}/../pccardd ${.CURDIR}/../pccardc

PROG=	dumpcis
MAN=	dumpcis.8
SRCS=	main.c dumpcis.c dumpcisfile.c readcis.c printcis.c

CFLAGS+= -I${.CURDIR}/../pccardd -I${.CURDIR}/../pccardc

.include <bsd.prog.mk>
