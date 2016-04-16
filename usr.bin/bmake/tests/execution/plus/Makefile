# $FreeBSD$

PACKAGE=	tests
FILESGROUPS=	TESTS
TESTSPACKAGE=	${PACKAGE}
TESTSDIR=	${TESTSBASE}/usr.bin/bmake/execution/plus

TAP_TESTS_SH=	legacy_test

FILESDIR=	${TESTSDIR}
FILES=		Makefile.test
FILES+=		expected.status.1
FILES+=		expected.stderr.1
FILES+=		expected.stdout.1

.include <bsd.test.mk>
