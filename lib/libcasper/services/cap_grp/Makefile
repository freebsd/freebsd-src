# $FreeBSD$

SHLIBDIR?=	/lib/casper

.include <src.opts.mk>

PACKAGE=libcasper
LIB=	cap_grp

SHLIB_MAJOR=	0
INCSDIR?=	${INCLUDEDIR}/casper

SRCS=	cap_grp.c

INCS=	cap_grp.h

LIBADD=	nv

CFLAGS+=-I${.CURDIR}

SUBDIR.${MK_TESTS}+= tests

.include <bsd.lib.mk>
