#	@(#)Makefile	8.1 (Berkeley) 6/2/93
# $FreeBSD: src/bin/ls/Makefile,v 1.9.2.2 2000/07/04 18:45:27 ache Exp $


PROG=	ls
SRCS=	cmp.c ls.c print.c util.c

.if !defined(RELEASE_BUILD_FIXIT)
CFLAGS+= -DCOLORLS
LDADD+= -ltermcap
DPADD+= ${LIBTERMCAP}
.endif

.include <bsd.prog.mk>
