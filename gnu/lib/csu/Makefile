# $FreeBSD$

GCCDIR=	${.CURDIR}/../../../contrib/gcc.295

.PATH: ${GCCDIR}

SRCS=		crtbegin.c crtend.c tm.h
OBJS=		${SRCS:N*.h:R:S/$/.o/g}
#OBJS+=		gcrt1.o
SOBJS=		crtbegin.So crtend.So
CFLAGS+=	-elf -Wall 
CFLAGS+=	-fkeep-inline-functions -finhibit-size-directive -fno-exceptions
CFLAGS+=	-I${GCCDIR}/config -I${GCCDIR} -I.
CFLAGS+=	-O0
LDFLAGS+=	-elf
NOMAN=		true
NOPIC=		true
NOPROFILE=	true
INTERNALLIB=	true
CLEANFILES+=	${SRCS}

all: ${OBJS} ${SOBJS}

gcrt1.o:	crt1.c
	${CC} ${CFLAGS} -DGCRT -c -o gcrt1.o ${.CURDIR}/crt1.c

crtbegin.c: crtstuff.c ${.CURDIR}/../../../lib/csu/common/crtbrand.c
	cat ${.ALLSRC} > ${.TARGET}

#crtbegin.c crtend.c: crtstuff.c
crtend.c: crtstuff.c
	ln -sf ${.ALLSRC} ${.TARGET}

#	KEEP THIS IN SYNC with src/gcc/usr.bin/cc/cc_tools/Makefile !!
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
