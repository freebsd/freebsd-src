#	@(#)Makefile	8.1 (Berkeley) 6/2/93
# $FreeBSD$


PROG=	ls
SRCS=	cmp.c ls.c print.c util.c
DPADD=	${LIBM}
LDADD=	-lm

.if !defined(RELEASE_BUILD_FIXIT)
CFLAGS+= -DCOLORLS
DPADD+=	${LIBTERMCAP}
LDADD+=	-ltermcap
.endif

.include <bsd.prog.mk>
