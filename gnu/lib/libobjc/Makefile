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

INCS=	encoding.h hash.h objc-api.h objc-list.h objc.h runtime.h \
	sarray.h thr.h typedstream.h NXConstStr.h Object.h Protocol.h
INCSDIR=${INCLUDEDIR}/objc

CFLAGS+= -I${OBJCDIR}/objc -I${OBJCDIR} -I.
CFLAGS+= -I${GCCDIR}/config -I${GCCDIR}

GENHDRS=	runtime-info.h tconfig.h tm.h 
CLEANFILES+=	${GENHDRS}
SRCS+=		${GENHDRS}

${OBJS}: ${GENHDRS}

runtime-info.h:
	`${CC} --print-prog-name=cc1obj` -print-objc-runtime-info \
	    < /dev/null > ${.TARGET}

tconfig.h:
	echo '#include "gansidecl.h"'				> ${.TARGET}
	echo '#include "${MACHINE_ARCH}/xm-${MACHINE_ARCH}.h"'	>> ${.TARGET}

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

.include <bsd.lib.mk>
