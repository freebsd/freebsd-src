# $FreeBSD$

.include "../Makefile.inc"
.PATH: ${DIAGPATH}/src ${DIAGPATH}/man

PROG=   perfquery
SRCS=   perfquery.c ibdiag_common.c
LIBADD= ibumad ibcommon ibmad
CFLAGS+= -I${DIAGPATH}/include
MAN=	perfquery.8

WARNS?= 1

.include <bsd.prog.mk>
