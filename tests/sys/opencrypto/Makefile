# $FreeBSD$

PACKAGE=	tests
FILESGROUPS=	TESTS
TESTSPACKAGE=	${PACKAGE}
TESTSDIR=	${TESTSBASE}/sys/opencrypto
BINDIR=		${TESTSDIR}

PLAIN_TESTS_SH=	runtests

TEST_METADATA.foo+=required_programs="python"
PYMODULES=	cryptodev.py cryptodevh.py cryptotest.py dpkt.py

FILESDIR=	${TESTSDIR}
FILES=		${PYMODULES}

.include <bsd.test.mk>
