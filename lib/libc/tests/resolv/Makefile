# $FreeBSD$

TESTSDIR=	${TESTSBASE}/lib/libc/resolv
BINDIR=		${TESTSDIR}

FILES+=		mach

ATF_TESTS_C+=	resolv_test
#TEST_METADATA.resolv_test=	timeout="1800"

# Note: this test relies on being dynamically linked.  You will get a
# spurious PASS for a statically linked test.
LIBADD.resolv_test+=	pthread

.include <bsd.test.mk>
