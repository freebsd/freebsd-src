#	@(#)Makefile	8.3 (Berkeley) 5/8/95
# $FreeBSD: src/bin/df/Makefile,v 1.9.2.1 2000/07/01 03:00:16 ps Exp $

PROG=	df
SRCS=	df.c vfslist.c
BINGRP=	operator
BINMODE=2555

MOUNT=	${.CURDIR}/../../sbin/mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

DPADD=	${LIBM}
LDADD=	-lm

.include <bsd.prog.mk>
