# $FreeBSD$

.include "../Makefile.inc"

.PATH: ../cc_tools ${GCCDIR}

SRCS=	multilib.h choose-temp.c obstack.c prefix.c pexecute.c version.c
.if ${OSVERSION} < 400004
SRCS+=	mkstemp.c
.endif

CFLAGS+=	-DPREFIX=\"/usr\"
CFLAGS+=	-DIN_GCC

LIB=		cc_drv
NOPROFILE=	YES
NOPIC=		YES

install:
	@true

${OUT_OBJ}.o ${OUT_OBJ}.so:	${OUT_FILE}

.include <bsd.lib.mk>
