# $FreeBSD$

GCCDIR=	${.CURDIR}/../../../contrib/gcc
.PATH: ${GCCDIR}

CCDIR=	${.CURDIR}/../../usr.bin/cc
.include "${CCDIR}/Makefile.tgt"

SRCS=		crtstuff.c tconfig.h
OBJS=		crtbegin.o crtend.o
SOBJS=		crtbegin.So crtend.So
CFLAGS+=	-DIN_GCC -DHAVE_LD_EH_FRAME_HDR
CFLAGS+=	-finhibit-size-directive -fno-inline-functions \
		-fno-exceptions -fno-omit-frame-pointer
CFLAGS+=	-I${GCCDIR}/config -I${GCCDIR} -I. \
		-I${CCDIR}/cc_tools
CRTS_CFLAGS=	-DCRTSTUFFS_O ${PICFLAG}
MKDEPCMD=	CC="${CC}" MKDEP_CPP_OPTS="-M -DCRT_BEGIN" mkdep

.if ${TARGET_ARCH} == "powerpc"
.PATH: ${GCCDIR}/config/${GCC_CPU}
TGTOBJS=	crtsavres.o
SRCS+=		crtsavres.asm
.endif
.if ${TARGET_ARCH} == "sparc64"
.PATH: ${GCCDIR}/config/${GCC_CPU}
TGTOBJS=	crtfastmath.o
SRCS+=		crtfastmath.c
.endif

all: ${OBJS} ${SOBJS} ${TGTOBJS}

CLEANFILES=	${OBJS} ${SOBJS} ${TGTOBJS}

crtbegin.o crtbegin.So crtend.o crtend.So: ${SRCS}

crtbegin.o:
	${CC} ${CFLAGS} -g0 -DCRT_BEGIN \
		-c -o ${.TARGET} ${.ALLSRC:M*crtstuff*}

crtbegin.So:
	${CC} ${CFLAGS} -g0 -DCRT_BEGIN ${CRTS_CFLAGS} \
		-c -o ${.TARGET} ${.ALLSRC:M*crtstuff*}

crtend.o:
	${CC} ${CFLAGS} -g0 -DCRT_END \
		-c -o ${.TARGET} ${.ALLSRC:M*crtstuff*}

crtend.So:
	${CC} ${CFLAGS} -g0 -DCRT_END ${CRTS_CFLAGS} \
		-c -o ${.TARGET} ${.ALLSRC:M*crtstuff*}

CLEANFILES+=	tconfig.h
tconfig.h: ${CCDIR}/cc_tools/Makefile
	${MAKE} -f ${.ALLSRC} MFILE=${.ALLSRC} GCCDIR=${GCCDIR} ${.TARGET}

realinstall:
.for file in ${OBJS} ${SOBJS} ${TGTOBJS}
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${file} ${DESTDIR}${LIBDIR}/${file:S/.So$/S.o/}
.endfor

.include <bsd.lib.mk>
