PACKAGE=	tests

ATF_TESTS_SH+=	basic
# Tests reuse jail names and so cannot run in parallel.
TEST_METADATA.basic+=	is_exclusive=true

${PACKAGE}FILES+= utils.subr

.include <bsd.test.mk>
