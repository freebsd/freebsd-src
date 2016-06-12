# $FreeBSD$

.include "../Makefile.inc"
.PATH: ${DIAGPATH}/src ${DIAGPATH}/man

PROG=   ibsendtrap
SRCS=   ibsendtrap.c ibdiag_common.c
LIBADD= ibmad
CFLAGS+= -I${DIAGPATH}/include
MAN=

WARNS?= 1

.include <bsd.prog.mk>
