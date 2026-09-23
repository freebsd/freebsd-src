PACKAGE=	tests

TESTSDIR=		${TESTSBASE}/sys/vfs

ATF_TESTS_C+=	lookup_cap_dotdot
CFLAGS.lookup_cap_dotdot.c+=	-I${SRCTOP}/tests

ATF_TESTS_C+=	nmount_check_fsid
CFLAGS.nmount_check_fsid.c+=	-I${SRCTOP}/tests

ATF_TESTS_SH+=	abi_root_symlink

LDFLAGS=	-lutil

#ATF_TESTS_SH+=	lookup_test

TAP_TESTS_SH+=	trailing_slash

.include <bsd.test.mk>
