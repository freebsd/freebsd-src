#	@(#)Makefile	8.1 (Berkeley) 5/31/93

.include <src.opts.mk>

PACKAGE=runtime
PROG=	cp
SRCS=	cp.c utils.c
CFLAGS+= -D_ACL_PRIVATE

HAS_TESTS=
SUBDIR.${MK_TESTS}=	tests

.include <bsd.prog.mk>
