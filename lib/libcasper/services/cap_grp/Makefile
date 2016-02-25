# $FreeBSD$

LIB=	cap_grp

SHLIB_MAJOR=	0
SHLIBDIR?=	/lib/casper
INCSDIR?=	${INCLUDEDIR}/casper

SRCS=	cap_grp.c

INCS=	cap_grp.h

LIBADD=	nv

CFLAGS+=-I${.CURDIR}
CFLAGS+=-I${.CURDIR}/../libcasper

WARNS?=	6

.include <bsd.lib.mk>
