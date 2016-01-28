# $FreeBSD$

.include "../Makefile.inc"
.PATH: ${DIAGPATH}/src ${DIAGPATH}/man

PROG=   ibsysstat
SRCS=   ibsysstat.c ibdiag_common.c
LIBADD= ibumad ibcommon ibmad
CFLAGS+= -I${DIAGPATH}/include
MAN=	ibsysstat.8

WARNS?= 1

.include <bsd.prog.mk>
