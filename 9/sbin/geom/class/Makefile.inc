# $FreeBSD$

SHLIBDIR?=${GEOM_CLASS_DIR}
SHLIB_NAME?=geom_${GEOM_CLASS}.so
LINKS=	${BINDIR}/geom ${BINDIR}/g${GEOM_CLASS}
MAN=	g${GEOM_CLASS}.8
SRCS+=	geom_${GEOM_CLASS}.c subr.c

CFLAGS+= -I${.CURDIR}/../..

.include "../Makefile.inc"
