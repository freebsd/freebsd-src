#	Makefile for pccardd
# $FreeBSD$

PROG=	pccardd
SRCS=	pccardd.c cardd.c file.c util.c readcis.c printcis.c server.c
MAN=	pccard.conf.5 pccardd.8
DPADD=	${LIBUTIL}
LDADD=	-lutil 
CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../pccardc

.include <bsd.prog.mk>
.PATH:	${.CURDIR}/../pccardc
