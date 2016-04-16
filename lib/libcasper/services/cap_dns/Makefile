# $FreeBSD$

PACKAGE=libcasper
LIB=	cap_dns

SHLIB_MAJOR=	0
SHLIBDIR?=	/lib/casper
INCSDIR?=	${INCLUDEDIR}/casper

SRCS=	cap_dns.c

INCS=	cap_dns.h

LIBADD=	nv

CFLAGS+=-I${.CURDIR}

.include <bsd.lib.mk>
