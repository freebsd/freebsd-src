# $NetBSD: Makefile,v 1.11 2013/04/12 17:18:11 christos Exp $

NOMAN=		# defined

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libpthread
LDADD+=		-lpthread
LDADD.t_fpu+=	-lm

.include "${.PARSEDIR}/../csu/Makefile.check_stack"

SRCS.t_join=	t_join.c ${SRCS_CHECK_STACK}
CPPFLAGS.t_join.c+=	${CPPFLAGS_CHECK_STACK}
CPPFLAGS.t_condwait.c+=	-I${.CURDIR}/../libc/gen

TESTS_SH+=	t_atexit
TESTS_C+=	t_barrier
TESTS_SH+=	t_cancel
TESTS_C+=	t_cond
TESTS_C+=	t_condwait
TESTS_C+=	t_detach
TESTS_C+=	t_equal
TESTS_SH+=	t_exit
TESTS_C+=	t_fork
TESTS_C+=	t_fpu
TESTS_C+=	t_join
TESTS_C+=	t_kill
TESTS_C+=	t_mutex
TESTS_C+=	t_name
TESTS_C+=	t_once
TESTS_C+=	t_preempt
TESTS_SH+=	t_resolv
TESTS_C+=	t_rwlock
TESTS_C+=	t_sem
TESTS_C+=	t_sigmask
TESTS_C+=	t_sigsuspend
TESTS_C+=	t_siglongjmp
TESTS_C+=	t_sleep
TESTS_C+=	t_swapcontext

LDADD.t_sem+=	-lrt

BINDIR=		${TESTSDIR}
PROGS=		h_atexit
PROGS+=		h_cancel
PROGS+=		h_exit
PROGS+=		h_resolv

FILESDIR=	${TESTSDIR}
FILES=		d_mach

SUBDIR=		dlopen

.include <bsd.test.mk>
