PACKAGE=		tests

TESTSDIR=		${TESTSBASE}/sys/mac/ipacl

ATF_TESTS_SH+=	ipacl_test

${PACKAGE}FILES+=	utils.subr

.for t in ${ATF_TESTS_SH}
TEST_METADATA.$t+=	required_kmods="mac_ipacl"
TEST_METADATA.$t+=	is_exclusive="true"
.endfor

.include <bsd.test.mk>
