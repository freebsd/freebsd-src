#	Makefile for pccardd
# $FreeBSD: src/usr.sbin/pccard/pccardd/Makefile,v 1.4.6.3 2003/07/19 19:37:19 imp Exp $

.PATH: ${.CURDIR}/../pccardc

PROG=	pccardd
MAN=	pccard.conf.5 pccardd.8
SRCS=	pccardd.c cardd.c file.c util.c readcis.c printcis.c server.c

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../pccardc

DPADD=	${LIBUTIL}
LDADD=	-lutil 

.include <bsd.prog.mk>
