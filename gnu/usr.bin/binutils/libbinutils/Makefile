#
# $FreeBSD$
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils

LIB=		binutils
SRCS+=		arlex.l arparse.y arsup.c bucomm.c debug.c filemode.c \
		ieee.c rdcoff.c rddbg.c rename.c stabs.c wrstabs.c version.c
.if ${TARGET_ARCH} == "mipseb"
CFLAGS+=	-DTARGET=\"elf32-bigmips\"
.elif ${TARGET_ARCH} == "mipsel"
CFLAGS+=	-DTARGET=\"elf32-littlemips\"
.else
CFLAGS+=	-DTARGET=\"${TARGET_ARCH}-unknown-freebsd\"
.endif
CFLAGS+=	-DVERSION=\"${VERSION}\"
CFLAGS+=	-I${SRCDIR}/binutils
CFLAGS+=	-I${SRCDIR}/bfd
NOPROFILE=	true
NOPIC=		true
INTERNALLIB=	true
INTERNALSTATICLIB=true

.include <bsd.lib.mk>
