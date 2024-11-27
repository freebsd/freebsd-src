ATF_TESTS_SH+=	syslogd_test
# Run in jail with new vnet so we don't need to worry about address conflicts
TEST_METADATA+=	execenv="jail"
TEST_METADATA+=	execenv_jail_params="vnet=new"
# Several syslogd listening on the same port prevent parallel operation
TEST_METADATA+=	is_exclusive="true"
TEST_METADATA+=	timeout=20

.include <bsd.test.mk>
