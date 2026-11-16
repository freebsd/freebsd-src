TESTSDIR=	${TESTSBASE}/sys/cpuctl

ATF_TESTS_C+=	cpuctl_ibs_stray_nmi_test

TEST_METADATA.cpuctl_ibs_stray_nmi_test+=	is_exclusive="true"
TEST_METADATA.cpuctl_ibs_stray_nmi_test+=	required_kmods="cpuctl hwpmc"
TEST_METADATA.cpuctl_ibs_stray_nmi_test+=	required_user="root"

.include <bsd.test.mk>
