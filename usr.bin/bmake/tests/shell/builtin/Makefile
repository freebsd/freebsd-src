# $FreeBSD$

PACKAGE=	tests
FILESGROUPS=	TESTS
TESTSPACKAGE=	${PACKAGE}
TESTSDIR=	${TESTSBASE}/usr.bin/bmake/shell/builtin

TAP_TESTS_SH=	legacy_test

FILESDIR=	${TESTSDIR}
FILES=		Makefile.test
FILES+=		expected.status.1
FILES+=		expected.status.2
FILES+=		expected.stderr.1
FILES+=		expected.stderr.2
FILES+=		expected.stdout.1
FILES+=		expected.stdout.2
FILES+=		sh

.include <bsd.test.mk>
