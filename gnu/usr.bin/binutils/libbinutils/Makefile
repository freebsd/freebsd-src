#
# $FreeBSD: src/gnu/usr.bin/binutils/libbinutils/Makefile,v 1.8.2.3 2001/08/01 22:54:11 obrien Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils

LIB=		binutils
SRCS+=		arlex.l arparse.y arsup.c bucomm.c debug.c filemode.c \
		ieee.c rdcoff.c rddbg.c rename.c stabs.c unwind-ia64.c \
		wrstabs.c version.c
CFLAGS+=	-DTARGET=\"${TARGET_ARCH}-unknown-freebsd\"
CFLAGS+=	-DVERSION=\"${VERSION}\"
CFLAGS+=	-I${SRCDIR}/binutils
CFLAGS+=	-I${SRCDIR}/bfd
NOPROFILE=	true
NOPIC=		true
INTERNALLIB=	true
INTERNALSTATICLIB=true

.include <bsd.lib.mk>
