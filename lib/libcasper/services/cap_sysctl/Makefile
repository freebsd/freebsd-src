# $FreeBSD$

LIB=	cap_sysctl

SHLIB_MAJOR=	0
SHLIBDIR?=	/lib/casper
INCSDIR?=	${INCLUDEDIR}/casper

SRCS=	cap_sysctl.c

INCS=	cap_sysctl.h

LIBADD=	nv

CFLAGS+=-I${.CURDIR}
CFLAGS+=-I${.CURDIR}/../libcasper

WARNS?=	6

.include <bsd.lib.mk>
