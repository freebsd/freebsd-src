NETBSD_ATF_TESTS_SH=	nonexistent_test
ATF_TESTS_SH+=		inet6

TEST_METADATA+=	execenv="jail"
TEST_METADATA+=	execenv_jail_params="vnet allow.raw_sockets"

.include <netbsd-tests.test.mk>
.include <bsd.test.mk>
