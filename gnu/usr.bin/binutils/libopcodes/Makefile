#
# $FreeBSD: src/gnu/usr.bin/binutils/libopcodes/Makefile,v 1.3.2.1 1999/08/29 14:32:14 peter Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/opcodes

LIB=		opcodes
SRCS+=		dis-buf.c disassemble.c
CFLAGS+=	-I${SRCDIR}/opcodes
NOPROFILE=	true
NOPIC=		true
INTERNALLIB=	true
INTERNALSTATICLIB=true

.include <bsd.lib.mk>
