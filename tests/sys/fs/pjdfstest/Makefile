TESTSDIR=	${TESTSBASE}/sys/fs/pjdfstest

PACKAGE=	tests

ATF_TESTS_SH+=	tmpfs	\
		ufs	\
		zfs

# Tests use an external resource.
TEST_METADATA.p9fs+=	is_exclusive=true

.include <bsd.test.mk>
