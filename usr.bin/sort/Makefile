# $FreeBSD$

.include <bsd.own.mk>

PROG=	sort

SRCS=	bwstring.c coll.c file.c mem.c radixsort.c sort.c vsort.c

sort.1: sort.1.in
	/usr/bin/sed ${MAN_SUB} ${.ALLSRC} >${.TARGET}

CLEANFILES+= sort.1

.if defined(WITH_THREADS)
CFLAGS+= -DSORT_THREADS
LDFLAGS+= -lpthread -lmd
MAN_SUB+= -e 's|%%THREADS%%||g'
.else
LDFLAGS+= -lmd
MAN_SUB+= -e 's|%%THREADS%%|\.\\"|g'
.endif

.if !defined(WITHOUT_NLS)
NLS+=	hu_HU.ISO8859-2
NLSSRCFILES= ${NLS:S@$@.msg@}
MAN_SUB+= -e 's|%%NLS%%||g'
.for lang in ${NLS}
NLSSRCDIR_${lang}= ${.CURDIR}/nls
.endfor
.else
CFLAGS+= -DWITHOUT_NLS
MAN_SUB+= -e 's|%%NLS%%|\.\\"|g'
.endif

.include <bsd.prog.mk>
