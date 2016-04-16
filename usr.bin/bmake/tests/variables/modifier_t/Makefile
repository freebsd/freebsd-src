# $FreeBSD$

PACKAGE=	tests
FILESGROUPS=	TESTS
TESTSPACKAGE=	${PACKAGE}
TESTSDIR=	${TESTSBASE}/usr.bin/bmake/variables/modifier_t

TAP_TESTS_SH=	legacy_test

FILESDIR=	${TESTSDIR}
FILES=		Makefile.test
FILES+=		expected.status.1
FILES+=		expected.status.2
FILES+=		expected.status.3
FILES+=		expected.stderr.1
FILES+=		expected.stderr.2
FILES+=		expected.stderr.3
FILES+=		expected.stdout.1
FILES+=		expected.stdout.2
FILES+=		expected.stdout.3

.include <bsd.test.mk>
