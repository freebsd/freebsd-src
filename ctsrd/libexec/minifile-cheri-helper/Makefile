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
LDADD+=		-lvuln_magic
LIBADD+=	z c_cheri

NO_SHARED=	yes

NEED_CHERI=	pure
WANT_DUMP=	yes

WARNS=4

#
# For libc files!
#
NO_WERROR=	yes

chsbrt.o:
	${AS} -G0 -o ${.TARGET} ${.ALLSRC}

.include <bsd.prog.mk>
