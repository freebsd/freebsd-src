# $FreeBSD$

SHLIBDIR?=	/usr/lib

.include <bsd.own.mk>

UMADDIR=	${.CURDIR}/../../management/libibumad
IBSRCDIR=	${UMADDIR}/src

.PATH: ${IBSRCDIR}

LIB=		ibumad
SHLIB_MAJOR=	1
NO_PROFILE=

SRCS=	umad.c

CFLAGS+= -DHAVE_CONFIG_H
CFLAGS+= -I${.CURDIR} -I/usr/include/infiniband

VERSION_MAP= ${IBSRCDIR}/libibumad.map

.include <bsd.lib.mk>
