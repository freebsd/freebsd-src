# $FreeBSD$

GCCDIR=	${.CURDIR}/../../../contrib/gcc
.PATH: ${GCCDIR}

SRCS=		crtstuff.c tconfig.h

OBJS=		crtbegin.o crtend.o
SOBJS=		crtbegin.So crtend.So
CFLAGS+=	-DIN_GCC -DHAVE_LD_EH_FRAME_HDR
CFLAGS+=	-finhibit-size-directive -fno-inline-functions \
		-fno-exceptions -fno-omit-frame-pointer
CFLAGS+=	-I${GCCDIR}/config -I${GCCDIR} -I. \
		-I${.CURDIR}/../../usr.bin/cc/cc_tools
CRTS_CFLAGS=	-DCRTSTUFFS_O ${PICFLAG}
MKDEPCMD=	CC=${CC} MKDEP_CPP_OPTS="-M -DCRT_BEGIN" mkdep
INTERNALLIB=	true

all: ${OBJS} ${SOBJS}

crtbegin.o crtbegin.So crtend.o crtend.So: ${SRCS}

crtbegin.o:
	${CC} ${CFLAGS} -g0 -DCRT_BEGIN \
		-c -o ${.TARGET} ${.ALLSRC:N*.h}

crtbegin.So:
	${CC} ${CFLAGS} -g0 -DCRT_BEGIN ${CRTS_CFLAGS} \
		-c -o ${.TARGET} ${.ALLSRC:N*.h}

crtend.o:
	${CC} ${CFLAGS} -g0 -DCRT_END \
		-c -o ${.TARGET} ${.ALLSRC:N*.h}

crtend.So:
	${CC} ${CFLAGS} -g0 -DCRT_END ${CRTS_CFLAGS} \
		-c -o ${.TARGET} ${.ALLSRC:N*.h}

CLEANFILES=	tconfig.h
tconfig.h: ${.CURDIR}/../../usr.bin/cc/cc_tools/Makefile
	${MAKE} -f ${.ALLSRC} MFILE=${.ALLSRC} ${.TARGET}

realinstall:
.for file in ${OBJS} ${SOBJS}
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${file} ${DESTDIR}${LIBDIR}/${file:S/.So$/S.o/}
.endfor

.include <bsd.lib.mk>
