#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

NOSHARED?=yes

PROG=	chflags
CFLAGS+=-Wall
.PATH:	${.CURDIR}/../../lib/libc/gen
SRCS=	chflags.c setflags.c

.include <bsd.prog.mk>
