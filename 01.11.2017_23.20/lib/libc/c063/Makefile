# $NetBSD: Makefile,v 1.1 2012/11/18 17:41:54 manu Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libc/c063

TESTS_C+=	t_faccessat
TESTS_C+=	t_fchmodat
TESTS_C+=	t_fchownat
TESTS_C+=	t_fexecve
TESTS_C+=	t_fstatat
TESTS_C+=	t_linkat
TESTS_C+=	t_mkdirat
TESTS_C+=	t_mkfifoat
TESTS_C+=	t_mknodat
TESTS_C+=	t_o_search
TESTS_C+=	t_openat
TESTS_C+=	t_readlinkat
TESTS_C+=	t_renameat
TESTS_C+=	t_symlinkat
TESTS_C+=	t_unlinkat
TESTS_C+=	t_utimensat

COPTS+= -D_INCOMPLETE_XOPEN_C063

.include <bsd.test.mk>
