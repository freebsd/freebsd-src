# $FreeBSD: src/bin/df/Makefile,v 1.7.2.1 1999/08/29 14:12:13 peter Exp $
#	@(#)Makefile	8.3 (Berkeley) 5/8/95

PROG=	df
SRCS=	df.c vfslist.c
BINGRP=	operator
BINMODE=2555

MOUNT=	${.CURDIR}/../../sbin/mount
CFLAGS+= -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
