TESTSDIR=	${TESTSBASE}/sys/pmc

ATF_TESTS_C+=	pmc_api_test
ATF_TESTS_C+=	pmc_credexec_test
ATF_TESTS_C+=	pmc_detach_test
ATF_TESTS_C+=	pmc_exec_test
ATF_TESTS_C+=	pmc_lifecycle_test
ATF_TESTS_C+=	pmc_wrap_test

LIBADD+=	pmc
LIBADD.pmc_detach_test+=	pthread

.include <bsd.test.mk>
