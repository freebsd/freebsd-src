#
# $Id: Makefile,v 1.2 1999/03/05 04:55:03 obrien Exp $
#
.include "../Makefile.inc"

.PATH: ../cc_tools ${GCCDIR} 

SRCS=	multilib.h choose-temp.c obstack.c prefix.c pexecute.c version.c
#	XXX change choose-temp.c to use stock mkstemp()
SRCS+=	mkstemp.c

CFLAGS+=	-DPREFIX=\"/usr\"
CFLAGS+=	-DIN_GCC

LIB=		cc_drv
NOPROFILE=	YES
NOPIC=		YES

install:
	@true

${OUT_OBJ}.o ${OUT_OBJ}.so:	${OUT_FILE}

.include <bsd.lib.mk>
