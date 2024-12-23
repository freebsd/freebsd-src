ATF_TESTS_SH=	syslogd_test
# Run in jail with new vnet so we don't need to worry about address conflicts
TEST_METADATA=	execenv="jail" \
		execenv_jail_params="vnet=new"
		timeout=20

.include <bsd.test.mk>
