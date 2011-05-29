# $FreeBSD$

.include "../Makefile.inc"
.PATH: ${DIAGPATH}/src ${DIAGPATH}/man

PROG=   sminfo
SRCS=   sminfo.c ibdiag_common.c
LDADD=	-libumad -libcommon -libmad
CFLAGS+= -I${DIAGPATH}/include
MAN=	sminfo.8

WARNS?= 1

.include <bsd.prog.mk>
