# $FreeBSD$

PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/opencrypto
BINDIR=		${TESTSDIR}

PLAIN_TESTS_SH=	runtests

TEST_METADATA.foo+=required_programs="python"
PYMODULES=	cryptodev.py cryptodevh.py cryptotest.py dpkt.py

${PACKAGE}FILES+=	${PYMODULES}

.include <bsd.test.mk>
