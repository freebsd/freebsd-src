# $FreeBSD: src/bin/df/Makefile,v 1.7.2.2 2000/01/23 03:19:33 mharo Exp $
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
