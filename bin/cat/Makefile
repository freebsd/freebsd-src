#	@(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD$

.include <src.opts.mk>

PACKAGE=runtime
PROG=	cat

.ifdef BOOTSTRAPPING
# For the bootstrap cat we disable all wide char support to allow building
# on Linux/macOS
CFLAGS+=-DBOOTSTRAP_CAT
.endif

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
