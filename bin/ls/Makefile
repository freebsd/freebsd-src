#	@(#)Makefile	8.1 (Berkeley) 6/2/93

.include <src.opts.mk>

PACKAGE=runtime
PROG=	ls
SRCS=	cmp.c ls.c print.c util.c
LIBADD=	util

.if ${MK_LS_COLORS} != no
CFLAGS+= -DCOLORLS
LIBADD+=	termcapw
.endif

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
