#	pccardc Makefile
#
# $FreeBSD: src/usr.sbin/pccard/dumpcis/Makefile,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../pccardd ${.CURDIR}/../pccardc

PROG=	dumpcis
MAN=	dumpcis.8
SRCS=	main.c dumpcis.c dumpcisfile.c readcis.c printcis.c

CFLAGS+= -I${.CURDIR}/../pccardd -I${.CURDIR}/../pccardc

.include <bsd.prog.mk>
