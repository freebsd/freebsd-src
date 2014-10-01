# $NetBSD: Makefile,v 1.4 2014/09/11 18:25:30 dholland Exp $
#

.include <bsd.own.mk>

TESTSDIR = ${TESTSBASE}/bin/sh

TESTS_SUBDIRS += dotcmd

TESTS_SH=	t_compexit
TESTS_SH+=	t_exit
TESTS_SH+=	t_expand
TESTS_SH+=	t_evaltested
TESTS_SH+=	t_fsplit
TESTS_SH+=	t_here
TESTS_SH+=	t_set_e
TESTS_SH+=	t_ulimit
TESTS_SH+=	t_varquote
TESTS_SH+=	t_wait

.include <bsd.test.mk>
