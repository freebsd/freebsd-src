# $FreeBSD$

SUBDIR+= doc

LIB=		gnuregex
SHLIB_MAJOR=	2

SRCS=	regex.c
INCSGROUPS=	INCS WRINCS
INCS=	regex.h
INCSDIR=	${INCLUDEDIR}/gnu
WRINCS=	gnuregex.h
NOMAN=	noman

CFLAGS+=-I${.CURDIR} -DHAVE_STRING_H=1 -DSTDC_HEADERS=1 -DNO_POSIX_COMPAT=1

.include <bsd.lib.mk>
