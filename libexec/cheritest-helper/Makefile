#
# $FreeBSD$
#
PROG=	cheritest-helper
SRCS=	cheritest-helper.c			\
	stub.c

INCS=	cheritest-helper.h

LDFLAGS+=	-Wl,--script=${.CURDIR}/sandbox.ld -nostdlib
MAN=
#STRIP=
DPADD=	${LIBMD} ${LIBC_CHERI}
LDADD=	-lmd -lc_cheri

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

.PATH: ${.CURDIR}/${MACHINE_ARCH}
.PATH: ${.CURDIR}/../../lib/libc/${MACHINE}/string
.PATH: ${.CURDIR}/../../lib/libc/string

.include <src.opts.mk>
.include <bsd.prog.mk>
