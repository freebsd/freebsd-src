#	@(#)Makefile	8.1 (Berkeley) 6/2/93
# $FreeBSD$


PROG=	ls
SRCS=	cmp.c ls.c print.c util.c lomac.c

.if !defined(RELEASE_CRUNCH)
CFLAGS+= -DCOLORLS
LDADD+= -ltermcap
DPADD+= ${LIBTERMCAP}
.endif

.include <bsd.prog.mk>
