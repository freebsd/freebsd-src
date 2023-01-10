# $FreeBSD$

.PATH: ${SRCTOP}/contrib/tzcode

PROG=	zdump
MAN=	zdump.8
SRCS=	zdump.c

CFLAGS+= -DNETBSD_INSPIRED=0 -DSTD_INSPIRED=1
CFLAGS+= -I${SRCTOP}/contrib/tzcode

.include <bsd.prog.mk>
