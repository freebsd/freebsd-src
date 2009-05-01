# $FreeBSD: src/sbin/geom/class/Makefile.inc,v 1.7.20.1 2009/04/15 03:14:26 kensmith Exp $

SHLIBDIR?=${CLASS_DIR}
SHLIB_NAME?=geom_${CLASS}.so
LINKS=	${BINDIR}/geom ${BINDIR}/g${CLASS}
MAN=	g${CLASS}.8
SRCS+=	geom_${CLASS}.c subr.c

CFLAGS+= -I${.CURDIR}/../..

.include "../Makefile.inc"
