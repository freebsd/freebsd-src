# $FreeBSD$

.include "../Makefile.inc"
.PATH: ${DIAGPATH}/src ${DIAGPATH}/man

PROG=   smpquery
SRCS=   smpquery.c ibdiag_common.c
LDADD=	-libumad -libcommon -libmad -losmcomp
CFLAGS+= -pthread -I${DIAGPATH}/include
MAN=	smpquery.8

WARNS?= 1

.include <bsd.prog.mk>
