# $FreeBSD$

.include "../Makefile.inc"

.PATH: ../cc_tools ${GCCDIR}

LIB=		cc_drv
INTERNALLIB=	YES

SRCS=	multilib.h choose-temp.c obstack.c prefix.c pexecute.c version.c

CFLAGS+=	-DIN_GCC

${OUT_OBJ}.o ${OUT_OBJ}.so:	${OUT_FILE}

.include <bsd.lib.mk>
