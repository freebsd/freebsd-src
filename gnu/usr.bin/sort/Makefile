# $FreeBSD$

SORTDIR= ${.CURDIR}/../../../contrib/gnu-sort
.PATH: ${SORTDIR}/lib ${SORTDIR}/src ${SORTDIR}/man

PROG=	sort
SRCS=   sort.c \
	argmatch.c \
	dup-safer.c \
	error.c \
	fopen-safer.c \
	getopt.c \
	getopt1.c \
	hard-locale.c \
	human.c \
	long-options.c \
	memcoll.c \
	physmem.c \
	posixver.c \
	quote.c \
	quotearg.c \
	version-etc.c \
	xmalloc.c \
	xmemcoll.c \
	xstrtoul.c \
	xstrtoumax.c

CFLAGS+=-I${.CURDIR} -I${SORTDIR}/lib -DHAVE_CONFIG_H \
	-DHAVE_LANGINFO_H=1 -DHAVE_NL_LANGINFO=1 -DLOCALEDIR=\"\"

.include <bsd.prog.mk>
