#
# $FreeBSD: src/gnu/usr.bin/binutils/libopcodes/Makefile,v 1.6.2.1 2000/07/06 22:16:09 obrien Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/opcodes

LIB=		opcodes
SRCS+=		dis-buf.c disassemble.c
CFLAGS+=	-I${SRCDIR}/opcodes -I${SRCDIR}/bfd
NOPROFILE=	true
NOPIC=		true
INTERNALLIB=	true
INTERNALSTATICLIB=true

# If set, BINUTILSDISTDIR is the path to a directory containing the full GNU
# binutils release. FreeBSD only distributes the bits that are required to
# build native architectures. BINUTILSDISTDIR is needed to build cross tools.
.if	defined(BINUTILSDISTDIR) && exists(${BINUTILSDISTDIR})
.PATH:		${BINUTILSDISTDIR}/bfd ${BINUTILSDISTDIR}/opcodes
CFLAGS+=	-I${BINUTILSDISTDIR}/opcodes -I${BINUTILSDISTDIR}/bfd
CFLAGS+=	-I${BINUTILSDISTDIR}/include
.endif

.include <bsd.lib.mk>
