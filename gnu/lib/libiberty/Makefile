# $FreeBSD: src/gnu/lib/libiberty/Makefile,v 1.4 2002/05/13 10:53:23 ru Exp $

SRCDIR=	${.CURDIR}/../../../contrib/binutils/libiberty

.PATH: ${SRCDIR} ${SRCDIR}/../include

#LIB=    iberty

# miniumally required sources
SRCS=	argv.c choose-temp.c concat.c cplus-dem.c fdmatch.c fnmatch.c getopt.c \
	getopt1.c getruntime.c hex.c floatformat.c objalloc.c obstack.c \
	pexecute.c spaces.c strerror.c strsignal.c xatexit.c xexit.c xmalloc.c \
	xstrdup.c xstrerror.c

CFLAGS+= -I${SRCDIR}/../include -I${.CURDIR}
CFLAGS+= -DHAVE_CONFIG_H

#INCS=	alloca-conf.h ansidecl.h demangle.h floatformat.h fnmatch.h getopt.h \
#	libiberty.h objalloc.h obstack.h
#INCSDIR=${INCLUDEDIR}/libiberty

.include <bsd.lib.mk>
