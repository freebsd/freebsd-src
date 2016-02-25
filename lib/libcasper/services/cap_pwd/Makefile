# $FreeBSD$

LIB=	cap_pwd

SHLIB_MAJOR=	0
SHLIBDIR?=	/lib/casper
INCSDIR?=	${INCLUDEDIR}/casper

SRCS=	cap_pwd.c

INCS=	cap_pwd.h

LIBADD=	nv

CFLAGS+=-I${.CURDIR}
CFLAGS+=-I${.CURDIR}/../libcasper

WARNS?=	6

.include <bsd.lib.mk>
