# $FreeBSD$

GCCDIR=		${.CURDIR}/../../../contrib/gcc

TARGET_ARCH?=	${MACHINE_ARCH}

.if ${TARGET_ARCH} == "alpha"
.PATH: ${GCCDIR}/config/alpha
SRCS=		crtbegin.s crtend.s
UGLYHACK=	-x assembler
.else
.PATH: ${GCCDIR}
SRCS=		crtstuff.c tm.h
.endif

OBJS=		crtbegin.o crtend.o
SOBJS=		crtbegin.So crtend.So
CFLAGS+=	-I${GCCDIR}/config -I${GCCDIR} -I. \
		-I${.CURDIR}/../../usr.bin/cc/cc_tools -I../../usr.bin/cc/cc_tools \
		-DIN_GCC \
		-finhibit-size-directive -fno-inline-functions \
		-fno-exceptions -fno-omit-frame-pointer
NOMAN=		true
NOPIC=		true
NOPROFILE=	true
INTERNALLIB=	true
CLEANFILES+=	tm.h

CRTS_CFLAGS=	-DCRTSTUFFS_O ${PICFLAG}

all: ${OBJS} ${SOBJS}

.if ${TARGET_ARCH} == "alpha"
crtbegin.o crtbegin.So:	crtbegin.s
crtend.o crtend.So:	crtend.s
.for PART in begin end
CLEANFILES+=	crt${PART}.s
crt${PART}.s:  crt${PART}.asm
	ln -sf ${.ALLSRC} ${.TARGET}
.endfor
.else
crtbegin.o crtbegin.So crtend.o crtend.So:	crtstuff.c tm.h
.endif

crtbegin.o:
	${CC} ${CFLAGS} -g0 -DCRT_BEGIN \
		-c -o ${.TARGET} ${UGLYHACK} ${.ALLSRC:N*.h}

crtbegin.So:
	${CC} ${CFLAGS} -g0 -DCRT_BEGIN ${CRTS_CFLAGS} \
		-c -o ${.TARGET} ${UGLYHACK} ${.ALLSRC:N*.h}

crtend.o:
	${CC} ${CFLAGS} -g0 -DCRT_END \
		-c -o ${.TARGET} ${UGLYHACK} ${.ALLSRC:N*.h}

crtend.So:
	${CC} ${CFLAGS} -g0 -DCRT_END ${CRTS_CFLAGS} \
		-c -o ${.TARGET} ${UGLYHACK} ${.ALLSRC:N*.h}

#	KEEP THIS IN SYNC with src/gnu/usr.bin/cc/cc_tools/Makefile !!
tm.h:
	echo '#include "gansidecl.h"'			> ${.TARGET}
	echo '#include "${MACHINE_ARCH}/${MACHINE_ARCH}.h"'	>> ${.TARGET}
.if ${MACHINE_ARCH} == "i386"
	echo '#include "${MACHINE_ARCH}/att.h"'		>> ${.TARGET}
.endif
	echo '#include <freebsd-native.h>'		>> ${.TARGET}
	echo '#include <freebsd.h>'			>> ${.TARGET}
.if exists(${MACHINE_ARCH}/config/${MACHINE_ARCH}/elf.h)
	echo '#include "${MACHINE_ARCH}/elf.h"'		>> ${.TARGET}
.endif
	echo '#include "${MACHINE_ARCH}/freebsd.h"'	>> ${.TARGET}
.if ${MACHINE_ARCH} == "i386"
	echo '#include "${MACHINE_ARCH}/perform.h"'	>> ${.TARGET}
.endif
	echo '#include "defaults.h"'			>> ${.TARGET}
	echo '#ifndef GENERATOR_FILE'			>> ${.TARGET}
	echo '#include "insn-codes.h"'			>> ${.TARGET}
	echo '#include "insn-flags.h"'			>> ${.TARGET}
	echo '#endif'					>> ${.TARGET}

realinstall:
.for file in ${OBJS} ${SOBJS}
	${INSTALL} ${COPY} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE} \
	    ${file} ${DESTDIR}${LIBDIR}/${file:S/.So$/S.o/}
.endfor

.include <bsd.lib.mk>
