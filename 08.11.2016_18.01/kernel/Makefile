# $NetBSD: Makefile,v 1.39 2016/02/15 14:59:38 christos Exp $

NOMAN=		# defined

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/kernel

TESTS_SUBDIRS=	kqueue
TESTS_C=	t_lock
TESTS_C+=	t_lockf
TESTS_C+=	t_pty
TESTS_C+=	t_mqueue
TESTS_C+=	t_sysv
TESTS_C+=	t_subr_prf
TESTS_C+=	t_kauth_pr_47598
TESTS_C+=	t_sysctl

TESTS_SH=	t_umount
TESTS_SH+=	t_umountstress
TESTS_SH+=	t_ps_strings

BINDIR=		${TESTSDIR}
PROGS=		h_ps_strings1
PROGS+=		h_ps_strings2

LDADD.t_mqueue+= -lrt


.if (${MKRUMP} != "no") && !defined(BSD_MK_COMPAT_FILE)
TESTS_SUBDIRS+=	tty

TESTS_C+=	t_extattrctl
TESTS_C+=	t_filedesc
TESTS_C+=	t_rnd
LDADD.t_extattrctl+= -lrumpvfs -lrump -lrumpuser
LDADD.t_extattrctl+= -lrump -lpthread
LDADD.t_filedesc+=  ${LDADD.t_rnd}
LDADD.t_rnd+=  -lrumpvfs -lrumpdev_rnd -lrumpdev -lrump -lrumpuser
LDADD.t_rnd+=  -lrump -lpthread
CPPFLAGS+=	-D_KERNTYPES

.endif


.PATH:			${NETBSDSRCDIR}/sys/kern
TESTS_C+=		t_extent
SRCS.t_extent=		t_extent.c subr_extent.c
CPPFLAGS.t_extent.c=	-D_EXTENT_TESTING -D__POOL_EXPOSE -D_KERNTYPES
CPPFLAGS.subr_extent.c=	-D_EXTENT_TESTING -D__POOL_EXPOSE -D_KERNTYPES

t_subr_prf.c: gen_t_subr_prf ${NETBSDSRCDIR}/sys/kern/subr_prf.c
	${HOST_SH} ${.ALLSRC} ${.TARGET}
CPPFLAGS.t_subr_prf.c=	-Wno-pointer-sign	# XXX platform vs kernel SHA2

CLEANFILES+=	t_subr_prf.c

.include <bsd.test.mk>
