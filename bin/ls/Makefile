#	@(#)Makefile	8.1 (Berkeley) 6/2/93
# $FreeBSD$

PROG=	ls
SRCS=	cmp.c lomac.c ls.c print.c util.c
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
