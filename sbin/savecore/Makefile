#	@(#)Makefile	8.2 (Berkeley) 4/17/94
# $FreeBSD$

PROG=	savecore
SRCS=	savecore.c zopen.c
MAN=	savecore.8

ZOPENPATH= ${.CURDIR}/../../usr.bin/compress
.PATH:	${ZOPENPATH}
CFLAGS+= -I${ZOPENPATH}

.include <bsd.prog.mk>
