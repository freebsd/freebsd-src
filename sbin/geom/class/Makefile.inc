# $FreeBSD$

.include "../Makefile.inc"

LIBDIR?=${CLASSDIR}
SHLIB_MAJOR=1

SRCS=   geom_${CLASS}.c subr.c
SHLIB_NAME?=geom_${CLASS}.so.${SHLIB_MAJOR}
LINKS=  ${BINDIR}/geom ${BINDIR}/g${CLASS}
MAN=	g${CLASS}.8

CFLAGS+=-I${.CURDIR}/../..
