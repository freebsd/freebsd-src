# $FreeBSD: src/sbin/geom/class/Makefile.inc,v 1.7.18.1 2008/11/25 02:59:29 kensmith Exp $

SHLIBDIR?=${CLASS_DIR}
SHLIB_NAME?=geom_${CLASS}.so
LINKS=	${BINDIR}/geom ${BINDIR}/g${CLASS}
MAN=	g${CLASS}.8
SRCS+=	geom_${CLASS}.c subr.c

CFLAGS+= -I${.CURDIR}/../..

.include "../Makefile.inc"
