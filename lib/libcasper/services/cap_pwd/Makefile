# $FreeBSD$

SHLIBDIR?=	/lib/casper

.include <src.opts.mk>

PACKAGE=libcasper
LIB=	cap_pwd

SHLIB_MAJOR=	0
INCSDIR?=	${INCLUDEDIR}/casper

SRCS=	cap_pwd.c

INCS=	cap_pwd.h

LIBADD=	nv

CFLAGS+=-I${.CURDIR}

SUBDIR.${MK_TESTS}+= tests

.include <bsd.lib.mk>
