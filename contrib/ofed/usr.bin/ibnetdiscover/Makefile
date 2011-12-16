# $FreeBSD$

.include "../Makefile.inc"
.PATH: ${DIAGPATH}/src ${DIAGPATH}/man

PROG=   ibnetdiscover
SRCS=   ibnetdiscover.c grouping.c ibdiag_common.c
LDADD=	-libumad -libcommon -libmad -losmcomp
CFLAGS+= -pthread -I${DIAGPATH}/include
MAN=	ibnetdiscover.8

WARNS?= 1

.include <bsd.prog.mk>
