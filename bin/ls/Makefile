#	@(#)Makefile	8.1 (Berkeley) 6/2/93
# $FreeBSD: src/bin/ls/Makefile,v 1.26.26.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	ls
SRCS=	cmp.c ls.c print.c util.c
DPADD=	${LIBUTIL}
LDADD=	-lutil

.if !defined(RELEASE_CRUNCH)
CFLAGS+= -DCOLORLS
DPADD+=	${LIBTERMCAP}
LDADD+=	-ltermcap
.endif

.include <bsd.prog.mk>
