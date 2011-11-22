# $FreeBSD$

PAMDIR=		${.CURDIR}/../../../../contrib/openpam

NO_INSTALLLIB=
NO_PROFILE=

CFLAGS+=	-I${PAMDIR}/include -I${.CURDIR}/../../libpam

SHLIB_NAME?=	${LIB}.so.${SHLIB_MAJOR}

.include "../Makefile.inc"
