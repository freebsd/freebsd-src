# $FreeBSD: src/gnu/usr.bin/binutils/libbinutils/Makefile,v 1.22.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils

LIB=	binutils
SRCS+=	arlex.l arparse.y arsup.c bucomm.c debug.c filemode.c \
	ieee.c rdcoff.c rddbg.c rename.c stabs.c unwind-ia64.c \
	wrstabs.c version.c binemul.c budemang.c emul_vanilla.c
WARNS?=	0
CFLAGS+= -DTARGET=\"${BINUTILS_ARCH}-obrien-freebsd\"
CFLAGS+= -DBFD_VERSION_STRING=\"${VERSION}\"
CFLAGS+= -D_GNU_SOURCE
CFLAGS+= -I${SRCDIR}/binutils
CFLAGS+= -I${SRCDIR}/bfd
INTERNALLIB=

.include <bsd.lib.mk>
