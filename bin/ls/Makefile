#	@(#)Makefile	8.1 (Berkeley) 6/2/93
# $FreeBSD: src/bin/ls/Makefile,v 1.24 2002/10/24 00:07:30 rwatson Exp $

PROG=	ls
SRCS=	cmp.c ls.c print.c util.c
NO_WERROR=1
WFORMAT=0
DPADD=	${LIBM}
LDADD=	-lm

.if !defined(RELEASE_CRUNCH)
CFLAGS+= -DCOLORLS
DPADD+=	${LIBTERMCAP}
LDADD+=	-ltermcap
.endif

.include <bsd.prog.mk>
