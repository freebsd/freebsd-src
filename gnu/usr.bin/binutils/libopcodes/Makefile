#
# $Id: Makefile,v 1.3 1998/03/12 05:14:19 jdp Exp $
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
