# $FreeBSD$

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils

LIB=	binutils
SRCS+=	arlex.l arparse.y arsup.c bucomm.c debug.c filemode.c \
	ieee.c rdcoff.c rddbg.c rename.c stabs.c unwind-ia64.c \
	wrstabs.c version.c binemul.c budemang.c emul_vanilla.c
WARNS=	0
CFLAGS+= -DTARGET=\"${TARGET_ARCH}-unknown-freebsd\"
CFLAGS+= -DBFD_VERSION_STRING=\"${VERSION}\"
CFLAGS+= -I${SRCDIR}/binutils
CFLAGS+= -I${SRCDIR}/bfd
INTERNALLIB=	true

.include <bsd.lib.mk>
