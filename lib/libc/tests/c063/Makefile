# $FreeBSD$

TESTSDIR=	${TESTSBASE}/lib/libc/c063

#TODO: t_o_search, t_utimensat

NETBSD_ATF_TESTS_C=	t_faccessat
NETBSD_ATF_TESTS_C+=	t_fchmodat
NETBSD_ATF_TESTS_C+=	t_fchownat
NETBSD_ATF_TESTS_C+=	t_fexecve
NETBSD_ATF_TESTS_C+=	t_fstatat
NETBSD_ATF_TESTS_C+=	t_linkat
NETBSD_ATF_TESTS_C+=	t_mkdirat
NETBSD_ATF_TESTS_C+=	t_mkfifoat
NETBSD_ATF_TESTS_C+=	t_mknodat
NETBSD_ATF_TESTS_C+=	t_openat
NETBSD_ATF_TESTS_C+=	t_readlinkat
NETBSD_ATF_TESTS_C+=	t_renameat
NETBSD_ATF_TESTS_C+=	t_symlinkat
NETBSD_ATF_TESTS_C+=	t_unlinkat

CFLAGS+= -D_INCOMPLETE_XOPEN_C063

.include <bsd.test.mk>
