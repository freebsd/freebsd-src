#
# $Id: Makefile,v 1.1 1999/04/04 16:36:34 obrien Exp $
#
.include "../Makefile.inc"

.PATH: ../cc_tools ${GCCDIR} 

SRCS=	multilib.h choose-temp.c obstack.c prefix.c pexecute.c version.c

CFLAGS+=	-DPREFIX=\"/usr\"
CFLAGS+=	-DIN_GCC

LIB=		cc_drv
NOPROFILE=	YES
NOPIC=		YES

install:
	@true

${OUT_OBJ}.o ${OUT_OBJ}.so:	${OUT_FILE}

.include <bsd.lib.mk>
