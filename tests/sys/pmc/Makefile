TESTSDIR=	${TESTSBASE}/sys/pmc

ATF_TESTS_C+=	pmc_api_test
ATF_TESTS_C+=	pmc_credexec_test
ATF_TESTS_C+=	pmc_detach_test
ATF_TESTS_C+=	pmc_exec_test
ATF_TESTS_C+=	pmc_lifecycle_test
ATF_TESTS_C+=	pmc_log_test
ATF_TESTS_C+=	pmc_pidtrack_test
ATF_TESTS_C+=	pmc_row_test
ATF_TESTS_C+=	pmc_ss_test
ATF_TESTS_C+=	pmc_unload_test
ATF_TESTS_C+=	pmc_wrap_test

ATF_TESTS_SH+=	pmc_callchain_ur_test
ATF_TESTS_SH+=	pmc_cli_test

.if ${MACHINE_ARCH} == "amd64"
ATF_TESTS_C+=	pmc_exterr_test
ATF_TESTS_C+=	pmc_mapfile_test
ATF_TESTS_C+=	pmc_perf_test
ATF_TESTS_C+=	pmc_rapl_test
ATF_TESTS_C+=	pmc_rdpmc_test

ATF_TESTS_SH+=	pmcstat_tsc_test

TESTS_SUBDIRS+=	amd
.endif

TEST_METADATA.pmc_exterr_test+=		required_user="root"
TEST_METADATA.pmc_callchain_ur_test+=	is_exclusive="true"
TEST_METADATA.pmc_cli_test+=		is_exclusive="true"
TEST_METADATA.pmc_pidtrack_test+=	is_exclusive="true"
TEST_METADATA.pmc_perf_test+=		is_exclusive="true"
TEST_METADATA.pmc_rapl_test+=		is_exclusive="true"
TEST_METADATA.pmc_rdpmc_test+=		is_exclusive="true"
TEST_METADATA.pmc_ss_test+=		is_exclusive="true"
TEST_METADATA.pmc_unload_test+=		is_exclusive="true"
TEST_METADATA.pmcstat_tsc_test+=	required_kmods="hwpmc"

LIBADD+=			pmc
LIBADD.pmc_detach_test+=	pthread
LIBADD.pmc_unload_test+=	memstat

CFLAGS.pmc_exterr_test.c+=	-I${SRCTOP}/tests

.include <bsd.test.mk>
