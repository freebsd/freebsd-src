# $FreeBSD$

# netlib gdtoa sources
.PATH: ${.CURDIR}/gdtoa

MISRCS+=_hdtoa.c _ldtoa.c glue.c
GDTOASRCS=dmisc.c dtoa.c gdtoa.c gethex.c gmisc.c \
	hd_init.c hexnan.c misc.c smisc.c \
	strtoIg.c strtod.c strtodg.c strtof.c strtord.c sum.c ulp.c
.if exists(${.CURDIR}/${MACHINE_ARCH}/stdlib/gdtoa.mk)
.include "${.CURDIR}/${MACHINE_ARCH}/stdlib/gdtoa.mk"
.endif

CFLAGS+=-I${.CURDIR}/../../contrib/gdtoa

.for src in ${GDTOASRCS}
MISRCS+=gdtoa_${src}
CLEANFILES+=gdtoa_${src}
gdtoa_${src}:
	ln -sf ${.CURDIR}/../../contrib/gdtoa/${src} ${.TARGET}
.endfor
