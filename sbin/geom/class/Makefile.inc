# $FreeBSD$

LIBDIR?= ${CLASSDIR}

SHLIB_MAJOR= 1
SHLIB_NAME?= geom_${CLASS}.so.${SHLIB_MAJOR}
LINKS=  ${BINDIR}/geom ${BINDIR}/g${CLASS}
MAN=	g${CLASS}.8
SRCS=   geom_${CLASS}.c subr.c

CFLAGS+= -I${.CURDIR}/../..

.include "../Makefile.inc"
