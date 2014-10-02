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

INCS=	cheritest-helper.h

LDFLAGS+=	-Wl,--script=${.CURDIR}/../../lib/libc_cheri/sandbox.ld \
		-nostdlib
MAN=
DPADD=	${LIBMD_CHERI} ${LIBC_CHERI}
LDADD=	-lmd_cheri -lc_cheri

USE_CHERI=	yes
USE_CHERI_STACK=	yes

NO_SHARED=	yes

FILESOWN=       ${LIBOWN}
FILESGRP=       ${LIBGRP}
FILESMODE=      ${LIBMODE}
FILESDIR=       ${LIBEXECDIR}
FILES=		cheritest-helper.bin cheritest-helper.dump
CLEANFILES=	cheritest-helper.bin cheritest-helper.dump

cheritest-helper.bin: cheritest-helper
	objcopy -S -O binary ${.ALLSRC} ${.TARGET}

cheritest-helper.dump: cheritest-helper
	objdump -xsSD ${.ALLSRC} > ${.TARGET}

.include <src.opts.mk>
.include <bsd.prog.mk>

CFLAGS+=	${CFLAGS.${.IMPSRC:T}}
CFLAGS.zutil.c=	-Wno-incompatible-pointer-types-discards-qualifiers
CFLAGS.inffast.c=	-O0
