# $FreeBSD$

.include "../Makefile.inc"
.PATH: ${DIAGPATH}/src ${DIAGPATH}/man

PROG=   smpdump
SRCS=   smpdump.c
LIBADD= ibumad ibcommon ibmad
CFLAGS+= -I${DIAGPATH}/include
MAN=	smpdump.8

WARNS?= 1

.include <bsd.prog.mk>
