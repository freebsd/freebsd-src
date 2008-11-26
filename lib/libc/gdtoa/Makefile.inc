# $FreeBSD: src/lib/libc/gdtoa/Makefile.inc,v 1.8.12.1 2008/10/02 02:57:24 kensmith Exp $

# netlib gdtoa sources
.PATH: ${.CURDIR}/gdtoa

MISRCS+=_hdtoa.c _ldtoa.c glue.c
GDTOASRCS+=dmisc.c dtoa.c gdtoa.c gethex.c gmisc.c \
	hd_init.c hexnan.c misc.c smisc.c \
	strtod.c strtodg.c strtof.c strtord.c sum.c ulp.c

CFLAGS+=-I${.CURDIR}/../../contrib/gdtoa

.for src in ${GDTOASRCS}
MISRCS+=gdtoa_${src}
CLEANFILES+=gdtoa_${src}
gdtoa_${src}:
	ln -sf ${.CURDIR}/../../contrib/gdtoa/${src} ${.TARGET}
.endfor
