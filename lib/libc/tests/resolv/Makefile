# $FreeBSD$

TESTSDIR=	${TESTSBASE}/lib/libc/resolv
BINDIR=		${TESTSDIR}

FILES+=		mach

ATF_TESTS_C+=	resolv_test

# Note: this test relies on being dynamically linked.  You will get a
# spurious PASS for a statically linked test.
DPADD.resolv_test+=	${LIBPTHREAD}
LDADD.resolv_test+=	-lpthread

.include <bsd.test.mk>
