#
# $FreeBSD$
#
PROG=	cheritest-helper
SRCS=	cheritest-helper.c			\
	stub.c

.PATH: ${.CURDIR}/../../lib/libz
SRCS+=	adler32.c \
	crc32.c \
	inffast.c \
	inflate.c \
	inftrees.c \
	zutil.c

INCS=	cheritest-helper.h cheritest-helper-internal.h

LDFLAGS+=	-Wl,--script=${.CURDIR}/../../lib/libc_cheri/sandbox.ld \
		-nostdlib
MAN=

LIBADD=	md_cheri c_cheri

USE_CHERI=	yes
USE_CHERI_STACK=	yes
WANT_DUMP=	yes

NO_SHARED=	yes

FILESOWN=       ${LIBOWN}
FILESGRP=       ${LIBGRP}
FILESMODE=      ${LIBMODE}
FILESDIR=       ${LIBEXECDIR}
FILES=		cheritest-helper.bin
CLEANFILES=	cheritest-helper.bin

cheritest-helper.bin: cheritest-helper
	objcopy -S -O binary ${.ALLSRC} ${.TARGET}

.include <src.opts.mk>
.include <bsd.prog.mk>

CFLAGS+=	${CFLAGS.${.IMPSRC:T}}
CFLAGS.zutil.c=	-Wno-incompatible-pointer-types-discards-qualifiers
CFLAGS.inffast.c=	-O0
