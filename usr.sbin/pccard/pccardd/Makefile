#	Makefile for pccardd
# $FreeBSD: src/usr.sbin/pccard/pccardd/Makefile,v 1.8.24.1 2008/10/02 02:57:24 kensmith Exp $

.PATH: ${.CURDIR}/../pccardc

PROG=	pccardd
MAN=	pccard.conf.5 pccardd.8
SRCS=	pccardd.c cardd.c file.c util.c readcis.c printcis.c server.c

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../pccardc

DPADD=	${LIBUTIL}
LDADD=	-lutil 

.include <bsd.prog.mk>
