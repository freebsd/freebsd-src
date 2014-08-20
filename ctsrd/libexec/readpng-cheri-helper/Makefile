# $FreeBSD$

PROG=	readpng-cheri-helper
SRCS=	readpng-cheri.c				\
	chsbrt.S				\
	execve.S				\
	setjmp.S				\
	sysarch.S

USE_CHERI=      yes
USE_CHERI_STACK=yes

.PATH: ${.CURDIR}/../../lib/libimagebox
SRCS+=	decode_png.c

MAN=

LDFLAGS+=	-Wl,--script=${.CURDIR}/sandbox.ld -nostdlib
NO_MAN=
#STRIP=
CFLAGS+=	-I${.CURDIR}/../../lib/libimagebox
CFLAGS+=	-I${.CURDIR}/../../../lib/libc/${MACHINE}
CFLAGS+=	-I${.CURDIR}/../../../lib/libc/include
CFLAGS+=	-DIMAGEBOX_PNG_NO_FD
LDADD+=		-lvuln_png -lz -lc_cheri

NO_SHARED=	yes

WARNS=4

FILESOWN=       ${LIBOWN}
FILESGRP=       ${LIBGRP}
FILESMODE=      ${LIBMODE}
FILESDIR=       ${LIBEXECDIR}
FILES=		${PROG}.bin ${PROG}.dump
CLEANFILES=	${PROG}.bin ${PROG}.dump

chsbrt.o:
	${AS} -G0 -o ${.TARGET} ${.ALLSRC}

${PROG}.bin: ${PROG}
	objcopy -S -O binary ${.ALLSRC} ${.TARGET}

${PROG}.dump: ${PROG}
	objdump -xsSD ${.ALLSRC} > ${.TARGET}

.PATH: ${.CURDIR}/${MACHINE_ARCH}
.PATH: ${.CURDIR}/../../../lib/libc/${MACHINE}/gen
.PATH: ${.CURDIR}/../../../lib/libc/${MACHINE}/string
.PATH: ${.CURDIR}/../../../lib/libc/${MACHINE}/sys
.PATH: ${.CURDIR}/../../../lib/libc/string

.include <src.opts.mk>
.include <bsd.prog.mk>
