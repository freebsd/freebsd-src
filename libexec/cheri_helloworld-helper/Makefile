#
# $FreeBSD$
#
PROG=	cheri_helloworld-helper
SRCS=	cheri_helloworld-helper.c

INCS=	cheri_helloworld-helper.h

LDFLAGS+=	-Wl,--script=${.CURDIR}/../../lib/libc_cheri/sandbox.ld \
		-nostdlib
MAN=
DPADD=	${LIBC_CHERI}
LDADD=	-lc_cheri

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
