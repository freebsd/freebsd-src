#
# $Id$
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/opcodes

LIB=		opcodes
SRCS=		i386-dis.c dis-buf.c disassemble.c
CFLAGS+=	-I${SRCDIR}/opcodes
NOPROFILE=	true
NOPIC=		true

install:

.include <bsd.lib.mk>
