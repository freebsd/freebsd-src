TESTSDIR=	${TESTSBASE}/sys/pmc/amd

ATF_TESTS_C+=	pmu_detect_test
ATF_TESTS_C+=	pmu_event_neg_test
ATF_TESTS_C+=	pmu_event_test
ATF_TESTS_C+=	pmu_mux_test
ATF_TESTS_C+=	rapl_domains_test
ATF_TESTS_SH+=	pmc_frontend_test

# These need the counters of cpu0 to themselves.
TEST_METADATA.pmu_event_neg_test+=	is_exclusive="true"
TEST_METADATA.pmu_event_test+=		is_exclusive="true"
TEST_METADATA.pmu_mux_test+=		is_exclusive="true"
TEST_METADATA.rapl_domains_test+=	is_exclusive="true"

LIBADD+=	pmc

.include <bsd.test.mk>
