#
# $Id: Makefile,v 1.5 1998/06/04 01:09:28 bde Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/binutils

LIB=		binutils
SRCS+=		arlex.l arparse.y arsup.c bucomm.c debug.c filemode.c \
		ieee.c rdcoff.c rddbg.c stabs.c wrstabs.c version.c
.if ${MACHINE_ARCH} == "mipseb"
CFLAGS+=	-DTARGET=\"elf32-bigmips\"
.elif ${MACHINE_ARCH} == "mipsel"
CFLAGS+=	-DTARGET=\"elf32-littlemips\"
.else
CFLAGS+=	-DTARGET=\"${MACHINE_ARCH}-unknown-freebsdelf\"
.endif
CFLAGS+=	-DVERSION=\"${VERSION}\"
CFLAGS+=	-I${SRCDIR}/binutils
CFLAGS+=	-I${SRCDIR}/bfd
NOPROFILE=	true
NOPIC=		true
INTERNALLIB=	true
INTERNALSTATICLIB=true

.include <bsd.lib.mk>
