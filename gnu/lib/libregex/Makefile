# $FreeBSD: src/gnu/lib/libregex/Makefile,v 1.31.2.1 2005/07/22 17:29:02 kensmith Exp $

SUBDIR=		doc

LIB=		gnuregex
SHLIB_MAJOR=	3

SRCS=		gnuregex.c
INCSGROUPS=	INCS WRINCS PXINCS
INCS=		regex.h.patched
INCSNAME=	regex.h
INCSDIR=	${INCLUDEDIR}/gnu
WRINCS=		gnuregex.h
PXINCS=		posix/regex.h
PXINCSDIR=	${INCSDIR}/posix

CFLAGS+=-DHAVE_CONFIG_H -I${.CURDIR}

CLEANFILES=	regex.h.patched gnuregex.c
regex.h.patched: regex.h
	sed 's=<posix/regex\.h>=<gnu/posix/regex.h>=g' \
		< ${.ALLSRC} > ${.TARGET}

gnuregex.c: regex.c
	sed 's=<regex\.h>=<gnu/regex.h>=g' \
		< ${.ALLSRC} > ${.TARGET}

.include <bsd.lib.mk>
