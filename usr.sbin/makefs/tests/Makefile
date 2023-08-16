
.include <src.opts.mk>

ATF_TESTS_SH+=	makefs_cd9660_tests
TEST_METADATA.makefs_cd9660_tests+=	required_files="/sbin/mount_cd9660"
ATF_TESTS_SH+=	makefs_ffs_tests
.if ${MK_ZFS} != "no"
ATF_TESTS_SH+=	makefs_zfs_tests
# ZFS pools created by makefs always have the same GUID, so OpenZFS
# refuses to import more than one at a time.  Thus the ZFS tests cannot
# be run in parallel for now.
TEST_METADATA.makefs_zfs_tests+=	is_exclusive="true"
.endif

BINDIR=		${TESTSDIR}

# XXX: PACKAGE support for SCRIPTS
SCRIPTS+=	makefs_tests_common.sh
SCRIPTSNAME_makefs_tests_common.sh=	makefs_tests_common.sh

.for t in ${ATF_TESTS_SH}
TEST_METADATA.$t+=	required_user="root"
.endfor

.include <bsd.test.mk>
