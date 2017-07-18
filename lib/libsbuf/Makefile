# $FreeBSD$

LIB=		sbuf
SHLIBDIR?= 	/lib
SRCS=		subr_prf.c subr_sbuf.c

SHLIB_MAJOR	= 6

SYMBOL_MAPS=    ${.CURDIR}/Symbol.map
VERSION_DEF=	${.CURDIR}/Version.def

.include <bsd.own.mk>

.PATH:	${SRCTOP}/sys/kern

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.lib.mk>
