#
# $FreeBSD$
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

.include <bsd.lib.mk>
