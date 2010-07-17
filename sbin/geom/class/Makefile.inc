# $FreeBSD: src/sbin/geom/class/Makefile.inc,v 1.7.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

SHLIBDIR?=${CLASS_DIR}
SHLIB_NAME?=geom_${CLASS}.so
LINKS=	${BINDIR}/geom ${BINDIR}/g${CLASS}
MAN=	g${CLASS}.8
SRCS+=	geom_${CLASS}.c subr.c

CFLAGS+= -I${.CURDIR}/../..

.include "../Makefile.inc"
