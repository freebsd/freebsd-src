TESTSDIR=		${TESTSBASE}/sys/kern/tty
BINDIR=			${TESTSDIR}

PLAIN_TESTS_PORCH+=	test_canon
PLAIN_TESTS_PORCH+=	test_canon_fullbuf
PLAIN_TESTS_PORCH+=	test_ncanon
PLAIN_TESTS_PORCH+=	test_recanon
ATF_TESTS_C+=		test_sti

PROGS+=			fionread
PROGS+=			readsz

LIBADD.test_sti=	util

.include <bsd.test.mk>
