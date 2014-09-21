#
# $FreeBSD$
#
PROG=	cheri_helloworld-helper
SRCS=	cheri_helloworld-helper.c

.PATH: ${.CURDIR}/../../lib/libc_cheri
SRCS+=	cheri_system_stub.c \
	cheri_fd_stub.c \
	fastmalloc.c

.PATH: ${.CURDIR}/../../lib/libcheri/mips64
SRCS+=	cheri_invoke.S

.PATH: ${.CURDIR}/../../lib/libc/string
.PATH: ${.CURDIR}/../../lib/libc/mips/string
CFLAGS+=	-I${.CURDIR}/../../lib/libc/include
SRCS+=	memcpy_c.s \
	memset_c.c \
	strlen.c


INCS=	cheri_helloworld-helper.h

LDFLAGS+=	-Wl,--script=${.CURDIR}/../../lib/libc_cheri/sandbox.ld \
		-nostdlib
MAN=

USE_CHERI=	yes
USE_CHERI_STACK=	yes

NO_SHARED=	yes

FILESOWN=       ${LIBOWN}
FILESGRP=       ${LIBGRP}
FILESMODE=      ${LIBMODE}
FILESDIR=       ${LIBEXECDIR}
FILES=		cheri_helloworld-helper.bin cheri_helloworld-helper.dump
CLEANFILES=	cheri_helloworld-helper.bin cheri_helloworld-helper.dump

cheri_helloworld-helper.bin: cheri_helloworld-helper
	objcopy -S -O binary ${.ALLSRC} ${.TARGET}

cheri_helloworld-helper.dump: cheri_helloworld-helper
	objdump -xsSD ${.ALLSRC} > ${.TARGET}

.include <src.opts.mk>
.include <bsd.prog.mk>

CFLAGS+=	-Wno-cast-align
