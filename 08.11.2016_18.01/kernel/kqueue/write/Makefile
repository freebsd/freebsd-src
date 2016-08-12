# $NetBSD: Makefile,v 1.1 2009/02/20 21:39:57 jmmv Exp $

NOMAN=		# defined

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/kernel/kqueue/write

TESTS_C=	t_fifo
TESTS_C+=	t_pipe
TESTS_C+=	t_ttypty

LDADD.t_ttypty=	-lutil

.include <bsd.test.mk>
