# $FreeBSD$

.include <src.opts.mk>

PACKAGE= runtime
PROG=	sort
SRCS=	bwstring.c coll.c file.c mem.c radixsort.c sort.c vsort.c
CSTD=	c11

sort.1: sort.1.in
	sed ${MAN_SUB} ${.ALLSRC} >${.TARGET}

CLEANFILES+= sort.1

.if ${MK_SORT_THREADS} != "no"
CFLAGS+= -DSORT_THREADS
LIBADD=	pthread md
MAN_SUB+= -e 's|%%THREADS%%||g'
.else
LIBADD=	md
MAN_SUB+= -e 's|%%THREADS%%|\.\\"|g'
.endif

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
