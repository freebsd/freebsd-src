#	@(#)Makefile	8.1 (Berkeley) 6/2/93
# $FreeBSD$


PROG=	ls
SRCS=	cmp.c setflags.c ls.c print.c util.c
.PATH:	${.CURDIR}/../../lib/libc/gen

.if !defined(RELEASE_BUILD_FIXIT)
CFLAGS+= -DCOLORLS
LDADD+= -ltermcap
DPADD+= ${LIBTERMCAP}
.endif

.include <bsd.prog.mk>
