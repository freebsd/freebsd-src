# $FreeBSD$

.include "../Makefile.inc"
.PATH: ${DIAGPATH}/src ${DIAGPATH}/man

PROG=   ibroute
SRCS=   ibroute.c ibdiag_common.c
LDADD=	-libumad -libcommon -libmad -losmcomp
CFLAGS+= -pthread -I${DIAGPATH}/include
MAN=	ibroute.8

WARNS?= 1

.include <bsd.prog.mk>
