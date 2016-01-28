# $FreeBSD$

.include "../Makefile.inc"
.PATH: ${DIAGPATH}/src ${DIAGPATH}/man

PROG=   ibstat
SRCS=   ibstat.c
LIBADD= ibumad ibcommon
CFLAGS+= -I${DIAGPATH}/include
MAN=	ibstat.8

WARNS?= 1

.include <bsd.prog.mk>
