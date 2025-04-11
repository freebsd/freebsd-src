ATF_TESTS_SH+=	traceroute_test

# Allow tests to run in parallel in their own jails
TEST_METADATA+=	execenv="jail"
TEST_METADATA+=	execenv_jail_params="vnet allow.raw_sockets"

.include <bsd.test.mk>
