# $FreeBSD$

BINDIR=			${TESTSDIR}

TAP_TESTS_PERL+=	gctl_test

PROGS+=			gctl
MAN=

DPADD.gctl+=		${LIBGEOM}
LDADD.gctl+=		-lgeom

.include <bsd.test.mk>
