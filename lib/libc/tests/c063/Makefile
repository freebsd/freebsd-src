# $FreeBSD$

#TODO: t_o_search

NETBSD_ATF_TESTS_C=	faccessat
NETBSD_ATF_TESTS_C+=	fchmodat
NETBSD_ATF_TESTS_C+=	fchownat
NETBSD_ATF_TESTS_C+=	fexecve
NETBSD_ATF_TESTS_C+=	fstatat
NETBSD_ATF_TESTS_C+=	linkat
NETBSD_ATF_TESTS_C+=	mkdirat
NETBSD_ATF_TESTS_C+=	mkfifoat
NETBSD_ATF_TESTS_C+=	mknodat
NETBSD_ATF_TESTS_C+=	openat
NETBSD_ATF_TESTS_C+=	readlinkat
NETBSD_ATF_TESTS_C+=	renameat
NETBSD_ATF_TESTS_C+=	symlinkat
NETBSD_ATF_TESTS_C+=	unlinkat
NETBSD_ATF_TESTS_C+=	utimensat

CFLAGS+= -D_INCOMPLETE_XOPEN_C063

.include "../Makefile.netbsd-tests"

.include <bsd.test.mk>
