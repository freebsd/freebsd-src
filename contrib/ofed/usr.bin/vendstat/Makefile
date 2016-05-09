# $FreeBSD$

.include "../Makefile.inc"
.PATH: ${DIAGPATH}/src ${DIAGPATH}/man

PROG=   vendstat
SRCS=   vendstat.c ibdiag_common.c
LIBADD= ibumad ibmad
CFLAGS+= -I${DIAGPATH}/include
MAN=	vendstat.8

WARNS?= 1

.include <bsd.prog.mk>
