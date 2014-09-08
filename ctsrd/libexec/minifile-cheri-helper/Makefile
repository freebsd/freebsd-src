# $FreeBSD$

PROG=	minifile-cheri-helper

.PATH: ${.CURDIR}/${MACHINE_ARCH}
SRCS=	minifile-cheri.c			\
	chsbrt.S				\
	stub.c					\
	sysarch.S

MAN=

LDFLAGS+=	-Wl,--script=${.CURDIR}/sandbox.ld -nostdlib -Wl,-dead_strip
NO_MAN=
#STRIP=
CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../../lib/libc/include
CFLAGS+=	-I${.CURDIR}/../../../lib/libc/mips
CFLAGS+=	-I${.CURDIR}/../../usr.bin/minifile
LDADD+=		-lvuln_magic -lz -lc_cheri

NO_SHARED=	yes

USE_CHERI=	yes
USE_CHERI_STACK=yes

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
CLEANFILES+=	${PROG}.bin ${PROG}.dump

chsbrt.o:
	${AS} -G0 -o ${.TARGET} ${.ALLSRC}

${PROG}.bin: ${PROG}
	${OBJCOPY} -S -O binary ${.ALLSRC} ${.TARGET}

${PROG}.dump: ${PROG}
	objdump -xsSD ${.ALLSRC} > ${.TARGET}

.include <src.opts.mk>
.include <bsd.prog.mk>
