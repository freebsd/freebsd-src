# $NetBSD: Makefile,v 1.1 2009/02/20 21:39:57 jmmv Exp $

NOMAN=		# defined

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/kernel/kqueue/read

TESTS_C=	t_fifo
TESTS_C+=	t_file
TESTS_C+=	t_file2
TESTS_C+=	t_pipe
TESTS_C+=	t_ttypty

LDADD.t_ttypty=	-lutil

.include <bsd.test.mk>
