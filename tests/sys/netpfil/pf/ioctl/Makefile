PACKAGE=	tests

TESTSDIR=       ${TESTSBASE}/sys/netpfil/pf/ioctl

# Allow tests to run in parallel in their own jails
TEST_METADATA+= execenv="jail"
TEST_METADATA+= execenv_jail_params="vnet"

ATF_TESTS_C += \
	validation

.include <bsd.test.mk>
