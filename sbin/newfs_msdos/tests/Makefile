TESTSRC=${SRCTOP}/contrib/netbsd-tests/sbin/newfs_msdos

NETBSD_ATF_TESTS_SH+=	create

ATF_TESTS_SH_SED_create=	-e 's/fsck_msdos/fsck_msdosfs/'

.include <netbsd-tests.test.mk>
.include <bsd.test.mk>
