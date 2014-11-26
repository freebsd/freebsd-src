#
# $FreeBSD$
#
PROG=	cheri_bench-helper
SRCS=	cheri_bench-helper.c

INCS=	cheri_bench-helper.h

LDFLAGS+=	-Wl,--script=${.CURDIR}/../../lib/libc_cheri/sandbox.ld \
		-nostdlib
MAN=

LIBADD=	c_cheri

USE_CHERI=	yes
USE_CHERI_STACK=	yes
WANT_DUMP=	yes

NO_SHARED=	yes

FILESOWN=       ${LIBOWN}
FILESGRP=       ${LIBGRP}
FILESMODE=      ${LIBMODE}
FILESDIR=       ${LIBEXECDIR}
FILES=		cheri_bench-helper.bin
CLEANFILES=	cheri_bench-helper.bin

cheri_bench-helper.bin: cheri_bench-helper
	objcopy -S -O binary ${.ALLSRC} ${.TARGET}

.include <src.opts.mk>
.include <bsd.prog.mk>
