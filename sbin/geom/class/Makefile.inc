# $FreeBSD: src/sbin/geom/class/Makefile.inc,v 1.6 2004/08/02 09:05:29 pjd Exp $

SHLIBDIR?=${CLASS_DIR}
SHLIB_NAME?=geom_${CLASS}.so
LINKS=  ${BINDIR}/geom ${BINDIR}/g${CLASS}
MAN=	g${CLASS}.8
SRCS=   geom_${CLASS}.c subr.c

CFLAGS+= -I${.CURDIR}/../..

.include "../Makefile.inc"
