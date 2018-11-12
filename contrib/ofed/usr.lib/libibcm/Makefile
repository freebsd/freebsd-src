# $FreeBSD$

SHLIBDIR?=	/usr/lib

.include <bsd.own.mk>
.include "../Makefile.inc"

IBCMDIR= 	${.CURDIR}/../../libibcm
IBSRCDIR=	${IBCMDIR}/src

.PATH: ${IBSRCDIR}

LIB=		ibcm
SHLIB_MAJOR=	1
MK_PROFILE=	no

SRCS= cm.c
LIBADD=	ibverbs
CFLAGS+=	-I${IBCMDIR}/include

MAN=
VERSION_MAP= ${IBSRCDIR}/libibcm.map

.include <bsd.lib.mk>
