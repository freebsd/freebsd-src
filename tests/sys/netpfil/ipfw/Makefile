PACKAGE=	tests

TESTSDIR=       ${TESTSBASE}/sys/netpfil/ipfw

ATF_TESTS_SH+=	fwd
${PACKAGE}FILES+=	fwd_inetd.conf

# Allow tests to run in parallel in their own jails
TEST_METADATA+= execenv="jail"
TEST_METADATA+= execenv_jail_params="vnet allow.raw_sockets"

.include <bsd.test.mk>
