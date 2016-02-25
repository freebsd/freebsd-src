# $FreeBSD$

LIB=	cap_random

SHLIB_MAJOR=	0
SHLIBDIR?=	/lib/casper
INCSDIR?=	${INCLUDEDIR}/casper

SRCS=	cap_random.c

INCS=	cap_random.h

LIBADD=	nv

CFLAGS+=-I${.CURDIR}
CFLAGS+=-I${.CURDIR}/../libcasper

WARNS?=	6

.include <bsd.lib.mk>
