#	@(#)Makefile	8.1 (Berkeley) 6/2/93
# $FreeBSD$


PROG=	ls
SRCS=	cmp.c setflags.c ls.c print.c util.c
.PATH:	${.CURDIR}/../../lib/libc/gen

CFLAGS+= -DCOLORLS
LDADD=  -lncurses

.include <bsd.prog.mk>
