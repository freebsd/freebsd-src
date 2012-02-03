# $FreeBSD$

# netlib gdtoa sources
.PATH: ${.CURDIR}/gdtoa

MISRCS+=_hdtoa.c _hldtoa.c _ldtoa.c glue.c
GDTOASRCS+=dmisc.c dtoa.c gdtoa.c gethex.c gmisc.c \
	hd_init.c hexnan.c misc.c smisc.c \
	strtod.c strtodg.c strtof.c strtord.c sum.c ulp.c

SYM_MAPS+=${.CURDIR}/gdtoa/Symbol.map

CFLAGS+=-I${.CURDIR}/../../contrib/gdtoa

.for src in ${GDTOASRCS}
MISRCS+=gdtoa_${src}
CLEANFILES+=gdtoa_${src}
gdtoa_${src}:
	ln -sf ${.CURDIR}/../../contrib/gdtoa/${src} ${.TARGET}
.endfor
