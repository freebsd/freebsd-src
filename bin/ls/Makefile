#	@(#)Makefile	8.1 (Berkeley) 6/2/93
# $FreeBSD$

.include <src.opts.mk>

PROG=	ls
SRCS=	cmp.c ls.c print.c util.c
DPADD=	${LIBUTIL}
LDADD=	-lutil

.if !defined(RELEASE_CRUNCH) && \
	${MK_LS_COLORS} != no
CFLAGS+= -DCOLORLS
DPADD+=	${LIBTERMCAPW}
LDADD+=	-ltermcapw
.endif

.include <bsd.prog.mk>
