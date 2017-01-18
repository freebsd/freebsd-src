# $NetBSD: Makefile,v 1.12 2014/10/15 21:55:34 justin Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libc/stdio

TESTS_C+=	t_clearerr
TESTS_C+=	t_fflush
TESTS_C+=	t_fmemopen
TESTS_C+=	t_fopen
TESTS_C+=	t_open_memstream
TESTS_C+=	t_fputc
TESTS_C+=	t_mktemp
TESTS_C+=	t_popen
TESTS_C+=	t_printf
TESTS_C+=	t_scanf
COPTS.t_printf.c += -Wno-format-nonliteral

LDADD.t_mktemp+=	-Wl,--no-fatal-warnings

.include <bsd.test.mk>
