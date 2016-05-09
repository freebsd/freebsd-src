# $FreeBSD$

.include "../Makefile.inc"
.PATH: ${DIAGPATH}/src ${DIAGPATH}/man

PROG=   ibaddr
SRCS=   ibaddr.c ibdiag_common.c
LIBADD= ibmad
CFLAGS+= -I${DIAGPATH}/include
MAN=	ibaddr.8

WARNS?= 1

.include <bsd.prog.mk>
