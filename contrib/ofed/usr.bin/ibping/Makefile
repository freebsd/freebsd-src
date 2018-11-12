# $FreeBSD$

.include "../Makefile.inc"
.PATH: ${DIAGPATH}/src ${DIAGPATH}/man

PROG=   ibping
SRCS=   ibping.c ibdiag_common.c
LIBADD= ibumad ibcommon ibmad
CFLAGS+= -I${DIAGPATH}/include
MAN=	ibping.8

WARNS?= 1

.include <bsd.prog.mk>
