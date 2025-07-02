ATF_TESTS_SH=	livedump_test log_test

# The test fails if any kernel modules are loaded while the test is running.
TEST_METADATA.livedump_test+= is_exclusive=true

.include <bsd.test.mk>
