#
# $Id: Makefile,v 1.1.1.1 1998/03/01 23:41:11 jdp Exp $
#

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/opcodes

LIB=		opcodes
SRCS+=		dis-buf.c disassemble.c
CFLAGS+=	-I${SRCDIR}/opcodes
NOPROFILE=	true
NOPIC=		true

install:

.include <bsd.lib.mk>
