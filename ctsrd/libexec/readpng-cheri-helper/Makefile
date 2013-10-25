# $FreeBSD$

PROG=	readpng-cheri-helper
SRCS=	readpng-cheri.c				\
	malloc.c				\
	stub.c					\
	subr_prf.c				\
	chsbrt.S				\
	strlen.S				\
	memcpy.S				\
	memcmp.c				\
	memset.c				\
	setjmp.S				\
	strcpy.c				\
	execve.S				\
	sysarch.S				\
	cerror.S

.PATH: ${.CURDIR}/../../lib/libimagebox
SRCS+=	decode_png.c

MAN=

LDFLAGS+=	-Wl,--script=${.CURDIR}/sandbox.ld -nostdlib
NO_MAN=
#STRIP=
CFLAGS+=	-I${.CURDIR}/../../lib/libimagebox
#CFLAGS+=	-I${.CURDIR}/../../libvuln_png
CFLAGS+=	-I${.CURDIR}/../../../lib/libc/${MACHINE}
CFLAGS+=	-I${.CURDIR}/../../../lib/libc/include
LDADD+=		-lvuln_png -lz

NO_SHARED=	yes

WARNS=4

#
# For libc files!
#
NO_WERROR=	yes

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

.include <bsd.prog.mk>
