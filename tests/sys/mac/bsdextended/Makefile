
TESTSDIR=	${TESTSBASE}/sys/mac/bsdextended

ATF_TESTS_SH+=	matches_test
TAP_TESTS_C+=	ugidfw_test

LIBADD.ugidfw_test+=	ugidfw

TEST_METADATA.ugidfw_test+=	required_user="root"
# Each test case of matches_test reuses the same ruleset number, so they cannot
# be run simultaneously
TEST_METADATA.matches_test+=	is_exclusive=true

.include <bsd.test.mk>
