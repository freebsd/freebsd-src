# $FreeBSD$

.include "../Makefile.inc"
# Keep the existing tests directory structure (with subdirs per component)
# rather than installing all of them to /usr/tests/lib/googletest
TESTSDIR=	${TESTSBASE}/lib/googletest/${.CURDIR:T}
