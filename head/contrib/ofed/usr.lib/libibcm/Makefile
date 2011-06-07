# $FreeBSD$

SHLIBDIR?=	/usr/lib

.include <bsd.own.mk>
.include "../Makefile.inc"

IBCMDIR= 	${.CURDIR}/../../libibcm
IBSRCDIR=	${IBCMDIR}/src

.PATH: ${IBSRCDIR}

LIB=		ibcm
SHLIB_MAJOR=	1
NO_PROFILE=

SRCS= cm.c
CFLAGS+=	-I ${IBCMDIR}/include

MAN=
VERSION_MAP= ${IBSRCDIR}/libibcm.map

.include <bsd.lib.mk>
