# $FreeBSD$

.include <src.opts.mk>

PACKAGE=lib${LIB}
LIB=	regex
SHLIB_MAJOR=	1
SHLIB_MINOR=	0

CFLAGS+=	-DLIBREGEX
LIBC_SRCTOP=	${.CURDIR:H}/libc
SYMBOL_MAPS=	${SYM_MAPS}
WARNS?=	2

#HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include "../Makefile.inc"
.include "${LIBC_SRCTOP}/regex/Makefile.inc"
.include <bsd.lib.mk>
