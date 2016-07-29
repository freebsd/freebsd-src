# $FreeBSD$

PACKAGE=libcasper
LIB=	cap_grp

SHLIB_MAJOR=	0
SHLIBDIR?=	/lib/casper
INCSDIR?=	${INCLUDEDIR}/casper

SRCS=	cap_grp.c

INCS=	cap_grp.h

LIBADD=	nv

CFLAGS+=-I${.CURDIR}

.include <bsd.lib.mk>
