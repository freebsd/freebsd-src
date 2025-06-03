PACKAGE=	tests

ATF_TESTS_SH=	syslogd_basic_format_test \
		syslogd_forwarded_format_test \
		syslogd_relayed_format_test \
		syslogd_test

# Run in jail with new vnet so we don't need to worry about address conflicts
TEST_METADATA=	execenv="jail" \
		execenv_jail_params="vnet=new" \
		timeout=20

${PACKAGE}FILES=syslogd_format_test_common.sh \
		syslogd_test_common.sh

.include <bsd.test.mk>
