# $FreeBSD$

LIB=	cap_dns

SHLIB_MAJOR=	0
SHLIBDIR?=	/lib/casper
INCSDIR?=	${INCLUDEDIR}/casper

SRCS=	cap_dns.c

INCS=	cap_dns.h

LIBADD=	nv

CFLAGS+=-I${.CURDIR}
CFLAGS+=-I${.CURDIR}/../libcasper

WARNS?=	6

.include <bsd.lib.mk>
