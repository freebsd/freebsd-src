# $FreeBSD$

.include "../Makefile.inc"
.PATH: ${DIAGPATH}/src ${DIAGPATH}/man

PROG=   ibtracert
SRCS=   ibtracert.c ibdiag_common.c
LIBADD= ibumad ibcommon ibmad osmcomp
CFLAGS+= -I${DIAGPATH}/include
MAN=	ibtracert.8

WARNS?= 1

.include <bsd.prog.mk>
