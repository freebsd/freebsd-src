# $FreeBSD$

GCCDIR=	${.CURDIR}/../../../contrib/gcc.295
.PATH: ${GCCDIR}

SRCS=		crtstuff.c tm.h
OBJS=		crtbegin.o crtend.o
SOBJS=		crtbegin.So crtend.So
CFLAGS+=	-I${GCCDIR}/config -I. -DIN_GCC \
		-finhibit-size-directive -fno-inline-functions \
		-fno-exceptions -fno-omit-frame-pointer
NOMAN=		true
NOPIC=		true
NOPROFILE=	true
INTERNALLIB=	true
CLEANFILES+=	tm.h

CRTS_CFLAGS=	-DCRTSTUFFS_O ${PICFLAG}

all: ${OBJS} ${SOBJS}

crtbegin.o:	crtstuff.c tm.h
	${CC} ${CFLAGS} -g0 -DCRT_BEGIN \
		-c -o ${.TARGET} ${GCCDIR}/crtstuff.c

crtbegin.So:	crtstuff.c tm.h
	${CC} ${CFLAGS} -g0 -DCRT_BEGIN ${CRTS_CFLAGS} \
		-c -o ${.TARGET} ${GCCDIR}/crtstuff.c

crtend.o:	crtstuff.c tm.h
	${CC} ${CFLAGS} -g0 -DCRT_END \
		-c -o ${.TARGET} ${GCCDIR}/crtstuff.c

crtend.So:	crtstuff.c tm.h
	${CC} ${CFLAGS} -g0 -DCRT_END ${CRTS_CFLAGS} \
		-c -o ${.TARGET} ${GCCDIR}/crtstuff.c

#	KEEP THIS IN SYNC with src/gnu/usr.bin/cc/cc_tools/Makefile !!
tm.h:
	echo '#include "${MACHINE_ARCH}/${MACHINE_ARCH}.h"'	> ${.TARGET}
.if ${MACHINE_ARCH} == "i386"
	echo '#include "${MACHINE_ARCH}/att.h"'			>> ${.TARGET}
.endif
	echo '#include <freebsd.h>'				>> ${.TARGET}
	echo '#include "${MACHINE_ARCH}/freebsd.h"'		>> ${.TARGET}
.if ${MACHINE_ARCH} == "i386"
	echo '#include "${MACHINE_ARCH}/perform.h"'		>> ${.TARGET}
.endif

realinstall:
.for file in ${OBJS} ${SOBJS}
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${file} ${DESTDIR}${LIBDIR}/${file:S/.So$/S.o/}
.endfor

.include <bsd.lib.mk>
