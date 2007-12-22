# $FreeBSD$

OBJCDIR=${.CURDIR}/../../../contrib/libobjc
GCCDIR=	${.CURDIR}/../../../contrib/gcc
GCCLIB=	${.CURDIR}/../../../contrib/gcclibs

.PATH: ${OBJCDIR}/objc ${OBJCDIR}

LIB=		objc
SHLIB_MAJOR=	3

SRCS=	archive.c class.c encoding.c gc.c hash.c init.c linking.m misc.c \
	nil_method.c NXConstStr.m Object.m objects.c Protocol.m sarray.c \
	selector.c sendmsg.c thr.c thr-objc.c exception.c

INCS=	encoding.h hash.h objc-api.h objc-decls.h objc-list.h objc.h runtime.h \
	sarray.h thr.h typedstream.h NXConstStr.h Object.h Protocol.h
INCSDIR=${INCLUDEDIR}/objc

CFLAGS+= -DHAVE_GTHR_DEFAULT -DIN_GCC -DIN_TARGET_LIBS
CFLAGS+= -I. -I${.CURDIR}/../../usr.bin/cc/cc_tools
CFLAGS+= -I${OBJCDIR}/objc -I${OBJCDIR}
CFLAGS+= -I${GCCDIR}/config -I${GCCDIR}
CFLAGS+= -I${GCCLIB}/include
CFLAGS+= -fexceptions -frandom-seed=RepeatabilityConsideredGood
OBJCFLAGS= -fgnu-runtime ${CFLAGS}

GENHDRS= runtime-info.h

runtime-info.h:
	`${CC} --print-prog-name=cc1obj` -print-objc-runtime-info \
	    < /dev/null > ${.TARGET}

.for H in tconfig.h tm.h config.h options.h gthr-default.h unwind.h
$H: ${.CURDIR}/../../usr.bin/cc/cc_tools/Makefile
	${MAKE} -f ${.ALLSRC} MFILE=${.ALLSRC} GCCDIR=${GCCDIR} ${.TARGET}
GENHDRS+=	$H
.endfor

CLEANFILES+=	${GENHDRS} cs-* optionlist
SRCS+=		${GENHDRS}

${OBJS}: ${GENHDRS}

.include <bsd.lib.mk>
