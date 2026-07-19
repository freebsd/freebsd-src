TESTSDIR=	${TESTSBASE}/sys/pmc

ATF_TESTS_C=	pmc_wrap_test
ATF_TESTS_C+=	pmc_detach_test

LIBADD+=	pmc
LIBADD.pmc_detach_test+=	pthread

.include <bsd.test.mk>
