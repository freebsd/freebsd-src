#	Makefile for pccardd
# $FreeBSD: src/usr.sbin/pccard/pccardd/Makefile,v 1.4.6.1 2000/08/26 15:49:48 ume Exp $

PROG=	pccardd
SRCS=	pccardd.c cardd.c file.c util.c readcis.c printcis.c server.c
MAN8=	pccardd.8
MAN5=	pccard.conf.5
DPADD=	${LIBUTIL}
LDADD=	-lutil 
CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../pccardc

.include <bsd.prog.mk>
.PATH:	${.CURDIR}/../pccardc
