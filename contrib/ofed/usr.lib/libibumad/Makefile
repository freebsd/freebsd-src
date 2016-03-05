# $FreeBSD$

SHLIBDIR?=	/usr/lib

.include <bsd.own.mk>
.include "../Makefile.inc"

IBSRCDIR=	${UMADDIR}/src

.PATH: ${IBSRCDIR}

LIB=		ibumad
SHLIB_MAJOR=	1
MK_PROFILE=	no

SRCS=	umad.c

LIBADD=	ibcommon
CFLAGS+= -DHAVE_CONFIG_H

VERSION_MAP= ${IBSRCDIR}/libibumad.map

.include <bsd.lib.mk>
