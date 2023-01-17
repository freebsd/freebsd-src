# $FreeBSD$

.PATH: ${SRCTOP}/contrib/tzcode

PROG=	zic
MAN=	zic.8
SRCS=	zic.c

CFLAGS+= -I${SRCTOP}/contrib/tzcode -include tzconfig.h

.include <bsd.prog.mk>
