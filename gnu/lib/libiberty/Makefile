# $FreeBSD$

SRCDIR=	${.CURDIR}/../../../contrib/binutils/libiberty

.PATH: ${SRCDIR}

LIB=    iberty
NOPIC=	true
NOMAN=	sorry

# miniumally required sources
SRCS=	argv.c choose-temp.c concat.c cplus-dem.c fdmatch.c fnmatch.c getopt.c \
	getopt1.c getruntime.c hex.c floatformat.c objalloc.c obstack.c \
	pexecute.c spaces.c strerror.c strsignal.c xatexit.c xexit.c xmalloc.c \
	xstrdup.c xstrerror.c

CFLAGS+= -I${SRCDIR}/../include -I${.CURDIR}
CFLAGS+= -DHAVE_CONFIG_H

HDRS=	alloca-conf.h ansidecl.h demangle.h floatformat.h fnmatch.h getopt.h \
	libiberty.h objalloc.h obstack.h

#beforeinstall:
#	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
#	    ${HDRS:S;^;${SRCDIR}/../include/;} ${DESTDIR}/usr/include/libiberty

.include <bsd.lib.mk>
