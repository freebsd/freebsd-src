# $FreeBSD$

.include "../Makefile.inc"
.PATH: ${DIAGPATH}/src ${DIAGPATH}/man

PROG=   ibaddr
SRCS=   ibaddr.c ibdiag_common.c
LDADD=	-libumad -libcommon -libmad
CFLAGS+= -I${DIAGPATH}/include
MAN=	ibaddr.8

WARNS?= 1

.include <bsd.prog.mk>
