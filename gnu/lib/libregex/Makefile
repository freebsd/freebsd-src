# $FreeBSD$

SUBDIR=		doc

LIB=		gnuregex
SHLIB_MAJOR=	2

SRCS=		gnuregex.c
INCSGROUPS=	INCS WRINCS PXINCS
INCS=		regex.h.patched
INCSNAME=	regex.h
INCSDIR=	${INCLUDEDIR}/gnu
WRINCS=		gnuregex.h
PXINCS=		posix/regex.h
PXINCSDIR=	${INCSDIR}/posix

CFLAGS+=-D_REGEX_RE_COMP -DSTDC_HEADERS

CLEANFILES=	regex.h.patched gnuregex.c
regex.h.patched: regex.h
	sed 's=<posix/regex\.h>=<gnu/posix/regex.h>=g' \
		< ${.ALLSRC} > ${.TARGET}

gnuregex.c: regex.c
	sed 's=<regex\.h>=<gnu/regex.h>=g' \
		< ${.ALLSRC} > ${.TARGET}

.include <bsd.lib.mk>
