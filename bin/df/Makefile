# $FreeBSD: src/bin/df/Makefile,v 1.9 1999/12/15 03:44:08 mharo Exp $
#	@(#)Makefile	8.3 (Berkeley) 5/8/95

PROG=	df
SRCS=	df.c vfslist.c
BINGRP=	operator
BINMODE=2555

MOUNT=	${.CURDIR}/../../sbin/mount
CFLAGS+= -I${MOUNT}
LDADD+= ${LIBM}
DPADD+= ${LIBM}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
