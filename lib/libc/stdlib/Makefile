# $NetBSD: Makefile,v 1.23 2012/04/04 10:52:59 joerg Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libc/stdlib

TESTS_C+=	t_abs
TESTS_C+=	t_atoi
TESTS_C+=	t_div
TESTS_C+=	t_getenv
TESTS_C+=	t_getenv_thread
TESTS_C+=	t_exit
TESTS_C+=	t_hsearch
TESTS_C+=	t_mi_vector_hash
TESTS_C+=	t_posix_memalign
TESTS_C+=	t_random
TESTS_C+=	t_strtod
TESTS_C+=	t_strtol
TESTS_C+=	t_system

TESTS_SH+=	t_atexit
TESTS_SH+=	t_getopt

MKMAN=no

BINDIR=		${TESTSDIR}

PROGS+=		h_atexit
PROGS+=		h_getopt h_getopt_long

LDADD.t_strtod=			-lm

LDADD.t_getenv_thread=		-lpthread

.include <bsd.test.mk>
