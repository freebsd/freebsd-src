#	Makefile for pccardd
# $FreeBSD$

PROG=	pccardd
SRCS=	pccardd.c cardd.c file.c util.c readcis.c printcis.c server.c
MAN8=	pccardd.8
MAN5=	pccard.conf.5
DPADD=	${LIBUTIL}
LDADD=	-lutil 
CFLAGS+=	-I${.CURDIR} -I${.CURDIR}/../pccardc

.include <bsd.prog.mk>
.PATH:	${.CURDIR}/../pccardc
