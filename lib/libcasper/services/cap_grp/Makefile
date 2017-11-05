# $FreeBSD$

SHLIBDIR?=	/lib/casper

.include <src.opts.mk>

PACKAGE=libcasper

SHLIB_MAJOR=	0
INCSDIR?=	${INCLUDEDIR}/casper

.if ${MK_CASPER} != "no"
SHLIB=	cap_grp

SRCS=	cap_grp.c
.endif

INCS=	cap_grp.h

LIBADD=	nv

CFLAGS+=-I${.CURDIR}

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.lib.mk>
