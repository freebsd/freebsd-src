# $FreeBSD$

SUBDIR+= doc

LIB=		gnuregex
SHLIB_MAJOR=	2

SRCS=	regex.c
INCSGROUPS=	INCS WRINCS PXINCS
INCS=	 regex.h.patched
CLEANFILES+=	regex.h.patched
INCSNAME=	regex.h
INCSDIR=	${INCLUDEDIR}/gnu
WRINCS=	gnuregex.h
PXINCS= posix/regex.h
PXINCSDIR=	${INCLUDEDIR}/gnu/posix
NOMAN=	noman

CFLAGS+=-D_REGEX_RE_COMP -DSTDC_HEADERS -I${DESTDIR}/usr/include/gnu

regex.h.patched: regex.h
	sed 's=<posix/regex\.h>=<gnu/posix/regex.h>=g' \
		< ${.ALLSRC} > ${.TARGET}

.include <bsd.lib.mk>
