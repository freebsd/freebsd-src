#	Makefile for pccardd
# $FreeBSD: src/usr.sbin/pccard/pccardd/Makefile,v 1.7 2001/07/20 06:20:07 obrien Exp $

PROG=	pccardd
MAN=	pccard.conf.5 pccardd.8
SRCS=	pccardd.c cardd.c file.c util.c readcis.c printcis.c server.c

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../pccardc

DPADD=	${LIBUTIL}
LDADD=	-lutil 

.include <bsd.prog.mk>
.PATH:	${.CURDIR}/../pccardc
