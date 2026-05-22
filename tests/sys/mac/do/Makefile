PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/mac/do

ATF_TESTS_SH+=	valid_configs invalid_configs consistency

${PACKAGE}FILES+=	common.sh

TEST_METADATA+=	execenv="jail"
TEST_METADATA+=	required_kmods="mac_do"
TEST_METADATA+=	required_user="root"

.include <bsd.test.mk>
