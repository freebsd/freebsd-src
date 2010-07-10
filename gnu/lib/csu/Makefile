# $FreeBSD$

.include <bsd.own.mk>
MK_SSP=		no

GCCDIR=	${.CURDIR}/../../../contrib/gcc
GCCLIB=	${.CURDIR}/../../../contrib/gcclibs
CCDIR=	${.CURDIR}/../../usr.bin/cc
.include "${CCDIR}/Makefile.tgt"

.PATH: ${GCCDIR}/config/${GCC_CPU} ${GCCDIR}

SRCS=		crtstuff.c tconfig.h tm.h options.h
OBJS=		crtbegin.o crtend.o crtbeginT.o
SOBJS=		crtbegin.So crtend.So
CSTD?=		gnu89
CFLAGS+=	-DIN_GCC -DHAVE_LD_EH_FRAME_HDR -DDT_CONFIG -D__GLIBC__=3
CFLAGS+=	-finhibit-size-directive -fno-inline-functions \
		-fno-exceptions -fno-zero-initialized-in-bss \
		-fno-zero-initialized-in-bss -fno-toplevel-reorder \
		-fno-asynchronous-unwind-tables -fno-omit-frame-pointer
CFLAGS+=	-I${GCCLIB}/include -I${GCCDIR}/config -I${GCCDIR} -I. \
		-I${CCDIR}/cc_tools
CRTS_CFLAGS=	-DCRTSTUFFS_O -DSHARED ${PICFLAG}
MKDEP=		-DCRT_BEGIN

.if ${MACHINE_ARCH} == "ia64"
BEGINSRC=	crtbegin.asm
ENDSRC=		crtend.asm
CFLAGS+=	-x assembler-with-cpp	# Ugly hack
CFLAGS+=	-include osreldate.h
.undef SRCS				# hack for 'make depend'
.endif
.if ${MACHINE_ARCH} == "powerpc" || ${MACHINE_ARCH} == "powerpc64"
TGTOBJS=	crtsavres.o
SRCS+=		crtsavres.asm
.endif
.if ${MACHINE_ARCH} == "sparc64"
TGTOBJS=	crtfastmath.o
SRCS+=		crtfastmath.c
.endif
BEGINSRC?=	crtstuff.c
ENDSRC?=	crtstuff.c

all: ${OBJS} ${SOBJS} ${TGTOBJS}
${OBJS} ${SOBJS}: ${SRCS:M*.h}

CLEANFILES=	${OBJS} ${SOBJS} ${TGTOBJS}

crtbegin.o:	${BEGINSRC}
	${CC} ${CFLAGS} -g0 -DCRT_BEGIN \
		-c -o ${.TARGET} ${.ALLSRC:N*.h}

crtbeginT.o:	${BEGINSRC}
	${CC} ${CFLAGS} -g0 -DCRT_BEGIN -DCRTSTUFFT_O \
		-c -o ${.TARGET} ${.ALLSRC:N*.h}

crtbegin.So:	${BEGINSRC}
	${CC} ${CFLAGS} -g0 -DCRT_BEGIN ${CRTS_CFLAGS} \
		-c -o ${.TARGET} ${.ALLSRC:N*.h}

crtend.o:	${ENDSRC}
	${CC} ${CFLAGS} -g0 -DCRT_END \
		-c -o ${.TARGET} ${.ALLSRC:N*.h}

crtend.So:	${ENDSRC}
	${CC} ${CFLAGS} -g0 -DCRT_END ${CRTS_CFLAGS} \
		-c -o ${.TARGET} ${.ALLSRC:N*.h}

CLEANFILES+=	tm.h tconfig.h options.h optionlist cs-tconfig.h cs-tm.h
tm.h tconfig.h options.h: ${CCDIR}/cc_tools/Makefile
	${MAKE} -f ${.ALLSRC} MFILE=${.ALLSRC} GCCDIR=${GCCDIR} ${.TARGET}

realinstall:
.for file in ${OBJS} ${SOBJS} ${TGTOBJS}
	${INSTALL} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${file} ${DESTDIR}${LIBDIR}/${file:S/.So$/S.o/}
.endfor

.include <bsd.lib.mk>
