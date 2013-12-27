#        @(#)Makefile	8.1 (Berkeley) 5/31/93
# $FreeBSD$

.include <bsd.own.mk>

PROG=	test
LINKS=	${BINDIR}/test ${BINDIR}/[
MLINKS=	test.1 [.1

.if ${MK_TESTS} != "no"
SUBDIR+=    tests
.endif

.include <bsd.prog.mk>
