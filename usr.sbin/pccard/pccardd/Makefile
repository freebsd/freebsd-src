#	Makefile for pccardd
# $FreeBSD$

PROG=	pccardd
MAN=	pccard.conf.5 pccardd.8
SRCS=	pccardd.c cardd.c file.c util.c readcis.c printcis.c server.c

CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../pccardc

DPADD=	${LIBUTIL}
LDADD=	-lutil 

.include <bsd.prog.mk>
.PATH:	${.CURDIR}/../pccardc
