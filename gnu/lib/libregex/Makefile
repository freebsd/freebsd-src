# $FreeBSD: src/gnu/lib/libregex/Makefile,v 1.24 2002/09/28 00:25:27 peter Exp $

SUBDIR+= doc

LIB=		gnuregex
SHLIB_MAJOR=	2

SRCS=	regex.c
INCS=	regex.h
INCSNAME= gnuregex.h
NOMAN=	noman

CFLAGS+=-I${.CURDIR} -DHAVE_STRING_H=1 -DSTDC_HEADERS=1 -DNO_POSIX_COMPAT=1

.include <bsd.lib.mk>
