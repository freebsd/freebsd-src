# $FreeBSD: src/sbin/geom/class/Makefile.inc,v 1.6.2.1 2005/04/29 08:00:46 pjd Exp $

SHLIBDIR?=${CLASS_DIR}
SHLIB_NAME?=geom_${CLASS}.so
LINKS=	${BINDIR}/geom ${BINDIR}/g${CLASS}
MAN=	g${CLASS}.8
SRCS+=	geom_${CLASS}.c subr.c

CFLAGS+= -I${.CURDIR}/../..

.include "../Makefile.inc"
