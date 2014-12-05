# $FreeBSD$

PROG=	readpng-cheri-helper
SRCS=	readpng-cheri.c				\
	execve.S				\
	sysarch.S

USE_CHERI=	yes
USE_CHERI_STACK=yes
WANT_DUMP=	yes

.PATH: ${.CURDIR}/../../lib/libimagebox
SRCS+=	decode_png.c

.PATH: ${.CURDIR}/../../../lib/libz
SRCS+=	adler32.c \
	crc32.c \
	inffast.c \
	inflate.c \
	inftrees.c \
	zutil.c

MAN=

LDFLAGS+=	-Wl,--script=${.CURDIR}/../../../lib/libc_cheri/sandbox.ld \
		-nostdlib
NO_MAN=
#STRIP=
CFLAGS+=	-I${.CURDIR}/../../lib/libimagebox
CFLAGS+=	-I${.CURDIR}/../../../lib/libc/${MACHINE}
CFLAGS+=	-I${.CURDIR}/../../../lib/libc/include
CFLAGS+=	-DIMAGEBOX_PNG_NO_FD
LDADD+=		-lvuln_png_cheri
LIBADD+=	c_cheri

NO_SHARED=	yes

WARNS=4

FILESOWN=       ${LIBOWN}
FILESGRP=       ${LIBGRP}
FILESMODE=      ${LIBMODE}
FILESDIR=       ${LIBEXECDIR}
FILES=		${PROG}.bin
CLEANFILES=	${PROG}.bin

${PROG}.bin: ${PROG}
	objcopy -S -O binary ${.ALLSRC} ${.TARGET}

.PATH: ${.CURDIR}/${MACHINE_ARCH}

.include <src.opts.mk>
.include <bsd.prog.mk>

CFLAGS+=	${CFLAGS.${.IMPSRC:T}}
CFLAGS.zutil.c=         -Wno-incompatible-pointer-types-discards-qualifiers
CFLAGS.inffast.c=	-O0
