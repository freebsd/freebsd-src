#	@(#)Makefile	8.2 (Berkeley) 4/17/94
# $FreeBSD: src/sbin/savecore/Makefile,v 1.3.8.1 2001/04/25 10:58:51 ru Exp $

PROG=	savecore
SRCS=	savecore.c zopen.c
MAN=	savecore.8

ZOPENPATH= ${.CURDIR}/../../usr.bin/compress
.PATH:	${ZOPENPATH}
CFLAGS+= -I${ZOPENPATH}

.include <bsd.prog.mk>
