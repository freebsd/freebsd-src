# $FreeBSD$

SHLIBDIR?=	/usr/lib

.include <bsd.own.mk>
.include "../Makefile.inc"

IBSRCDIR=	${IBCOMMONDIR}/src

.PATH: ${IBSRCDIR}

LIB=		ibcommon
SHLIB_MAJOR=	1
MK_PROFILE=	no

SRCS=	sysfs.c util.c hash.c stack.c time.c

VERSION_MAP= ${IBSRCDIR}/libibcommon.map

.include <bsd.lib.mk>
