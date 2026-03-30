SUBDIR+=	libpythagoras libdeep libval libval2 target
TESTS_SUBDIRS+=	rtld_deepbind

SUBDIR_DEPEND_libdeep=	libval2
SUBDIR_DEPEND_rtld_deepbind=	libval
SUBDIR_DEPEND_target=	libpythagoras

ATF_TESTS_C=	ld_library_pathfds
ATF_TESTS_C+=	ld_preload_fds
ATF_TESTS_C+=	set_var_test

.for t in ${ATF_TESTS_C}
SRCS.$t=	$t.c common.c
.endfor

ATF_TESTS_C+=	dlopen_test
ATF_TESTS_C+=	dlopen_hash_test

WARNS?=		3

.include <bsd.test.mk>
