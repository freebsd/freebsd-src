# $FreeBSD$

PACKAGE=lib${LIB}
LIB=		sbuf
SHLIBDIR?= 	/lib
SRCS=		subr_prf.c subr_sbuf.c

SHLIB_MAJOR	= 6

SYMBOL_MAPS=    ${.CURDIR}/Symbol.map
VERSION_DEF=	${.CURDIR}/Version.def

.PATH:	${.CURDIR}/../../sys/kern

.include <bsd.lib.mk>
