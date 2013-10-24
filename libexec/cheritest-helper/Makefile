#
# $FreeBSD$
#
PROG=	cheritest-helper
SRCS=	cheritest-helper.c			\
	malloc.c				\
	stub.c					\
	chsbrt.S				\
	memcpy.S				\
	memset.c

LDFLAGS+=	-Wl,--script=${.CURDIR}/sandbox.ld -nostdlib
NO_MAN=
#STRIP=
DPADD=	${LIBMD}
LDADD=	-lmd

NO_SHARED=	yes

#
# For libc files!
#
NO_WERROR=	yes

FILESOWN=       ${LIBOWN}
FILESGRP=       ${LIBGRP}
FILESMODE=      ${LIBMODE}
FILESDIR=       ${LIBEXECDIR}
FILES=		cheritest-helper.bin cheritest-helper.dump chsbrt.o
CLEANFILES=	cheritest-helper.bin cheritest-helper.dump chsbrt.o

chsbrt.o:
	${AS} -G0 -o ${.TARGET} ${.ALLSRC}

cheritest-helper.bin: cheritest-helper
	objcopy -S -O binary ${.ALLSRC} ${.TARGET}

cheritest-helper.dump: cheritest-helper
	objdump -xsSD ${.ALLSRC} > ${.TARGET}

.PATH: ${.CURDIR}/${MACHINE_ARCH}
.PATH: ${.CURDIR}/../../lib/libc/${MACHINE}/string
.PATH: ${.CURDIR}/../../lib/libc/string

.include <bsd.prog.mk>
