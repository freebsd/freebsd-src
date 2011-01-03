# $FreeBSD$

.include "../Makefile.inc"
.PATH: ${DIAGPATH}/src ${DIAGPATH}/man

PROG=   smpquery
SRCS=   smpquery.c ibdiag_common.c
LDADD=	-libumad -libcommon -libmad -losmcomp
CFLAGS+= -I${DIAGPATH}/include -I/usr/include/infiniband/
CFLAGS+= -pthread
MAN=	smpquery.8

WARNS?= 1

.include <bsd.prog.mk>
