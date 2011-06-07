# $FreeBSD$

.include "../Makefile.inc"
.PATH: ${DIAGPATH}/src ${DIAGPATH}/man

PROG=   ibstat
SRCS=   ibstat.c
LDADD=	-libumad -libcommon
CFLAGS+= -I${DIAGPATH}/include
MAN=	ibstat.8

WARNS?= 1

.include <bsd.prog.mk>
