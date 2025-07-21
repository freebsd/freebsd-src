.include <src.opts.mk>

ATF_TESTS_SH+=	makefs_cd9660_tests
TEST_METADATA.makefs_cd9660_tests+=	required_files="/sbin/mount_cd9660"
ATF_TESTS_SH+=	makefs_ffs_tests
ATF_TESTS_SH+=	makefs_msdos_tests
TEST_METADATA.makefs_msdos_tests+=	required_files="/sbin/mount_msdosfs"
.if ${MK_ZFS} != "no"
ATF_TESTS_SH+=	makefs_zfs_tests
.endif

BINDIR=		${TESTSDIR}

# XXX: PACKAGE support for SCRIPTS
SCRIPTS+=	makefs_tests_common.sh
SCRIPTSNAME_makefs_tests_common.sh=	makefs_tests_common.sh

.for t in ${ATF_TESTS_SH}
TEST_METADATA.$t+=	required_user="root"
.endfor

.include <bsd.test.mk>
