# $FreeBSD$

.include <src.opts.mk>

PACKAGE=libcasper

SHLIB_MAJOR=	0
SHLIBDIR?=	/lib/casper
INCSDIR?=	${INCLUDEDIR}/casper

.if ${MK_CASPER} != "no"
LIB=	cap_random

SRCS=	cap_random.c
.endif

INCS=	cap_random.h

LIBADD=	nv

CFLAGS+=-I${.CURDIR}

.include <bsd.lib.mk>
