#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/chflags/Makefile,v 1.8 2000/02/05 18:42:33 joe Exp $

NOSHARED?=yes

PROG=	chflags
CFLAGS+=-Wall
.PATH:	${.CURDIR}/../../lib/libc/gen
SRCS=	chflags.c setflags.c

.include <bsd.prog.mk>
