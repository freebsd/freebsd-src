# $FreeBSD: src/gnu/usr.bin/cc/cc_drv/Makefile,v 1.11 2000/01/24 08:44:30 obrien Exp $

.include "../Makefile.inc"

.PATH: ../cc_tools ${GCCDIR}

LIB=		cc_drv
NOPROFILE=	YES
NOPIC=		YES

SRCS=	multilib.h choose-temp.c obstack.c prefix.c pexecute.c version.c

CFLAGS+=	-DIN_GCC

install:
	@true

${OUT_OBJ}.o ${OUT_OBJ}.so:	${OUT_FILE}

.include <bsd.lib.mk>
