#	@(#)Makefile	8.2 (Berkeley) 4/17/94

PROG=	savecore
SRCS=	savecore.c zopen.c
MAN8=	savecore.8

ZOPENPATH= ${.CURDIR}/../../usr.bin/compress
.PATH:	${ZOPENPATH}
CFLAGS+= -I${ZOPENPATH}

.include <bsd.prog.mk>
