# $FreeBSD$

OBJCDIR=${.CURDIR}/../../../contrib/libobjc
GCCDIR=	${.CURDIR}/../../../contrib/gcc

.PATH: ${OBJCDIR}/objc ${OBJCDIR}

LIB=    objc
NOMAN=	sorry
NOPIC=	works but method lookup slowdown is significant

SRCS=   archive.c class.c encoding.c gc.c hash.c init.c misc.c \
	nil_method.c objects.c sarray.c selector.c sendmsg.c \
	thr.c thr-single.c \
	NXConstStr.m Object.m Protocol.m linking.m

HDRS=	encoding.h hash.h objc-api.h objc-list.h objc.h runtime.h \
	sarray.h thr.h typedstream.h NXConstStr.h Object.h Protocol.h

CFLAGS+= -fgnu-runtime -DHAVE_GTHR_DEFAULT -DIN_TARGET_LIBS
CFLAGS+= -I. -I${.CURDIR}/../../usr.bin/cc/cc_tools
CFLAGS+= -I${OBJCDIR}/objc -I${OBJCDIR} 
CFLAGS+= -I${GCCDIR}/config -I${GCCDIR}

beforeinstall:
	${INSTALL} -C -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${HDRS:S;^;${OBJCDIR}/objc/;} ${DESTDIR}/usr/include/objc

GENHDRS=	runtime-info.h tconfig.h
CLEANFILES+=	${GENHDRS}
SRCS+=		${GENHDRS}

${OBJS}: ${GENHDRS}

runtime-info.h:
	`${CC} --print-prog-name=cc1obj` -print-objc-runtime-info \
	    < /dev/null > ${.TARGET}

tconfig.h: ${.CURDIR}/../../usr.bin/cc/cc_tools/Makefile
	${MAKE} -f ${.ALLSRC} MFILE=${.ALLSRC} GCCDIR=${GCCDIR} ${.TARGET}

.include <bsd.lib.mk>
