# $FreeBSD$

.include "../Makefile.inc"
.PATH: ${DIAGPATH}/src ${DIAGPATH}/man

PROG=   ibroute
SRCS=   ibroute.c ibdiag_common.c
LIBADD= ibcommon ibmad osmcomp
CFLAGS+= -I${DIAGPATH}/include
MAN=	ibroute.8

WARNS?= 1

.include <bsd.prog.mk>
