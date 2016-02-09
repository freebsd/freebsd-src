#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

# clang crash (theoretically fixed, but fix doesn't build)
WANT_CHERI=none

PROG=	indent
SRCS=	indent.c io.c lexi.c parse.c pr_comment.c args.c

NO_WMISSING_VARIABLE_DECLARATIONS=

.include <bsd.prog.mk>
