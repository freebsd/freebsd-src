# $FreeBSD$

SHLIBDIR?=	/usr/lib

.include <bsd.own.mk>
.include "../Makefile.inc"

IBSRCDIR=	${UMADDIR}/src

.PATH: ${IBSRCDIR}

LIB=		ibumad
SHLIB_MAJOR=	1
NO_PROFILE=

SRCS=	umad.c

CFLAGS+= -DHAVE_CONFIG_H

VERSION_MAP= ${IBSRCDIR}/libibumad.map

.include <bsd.lib.mk>
