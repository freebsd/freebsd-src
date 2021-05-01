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

# Temporary disable building cat with Casper.
#.if ${MK_CASPER} != "no" && !defined(RESCUE) && !defined(BOOTSTRAPPING)
#LIBADD+=        casper
#LIBADD+=        cap_fileargs
#LIBADD+=        cap_net
#CFLAGS+=-DWITH_CASPER
#.endif
# Depend on Makefile to rebiuld when WITH_CASPER changes
cat.o:	Makefile

.include <bsd.prog.mk>
