#	$NetBSD: Makefile,v 1.18 2015/01/07 22:24:03 pooka Exp $
#

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/fs/ffs
WARNS=		4

PROGS=			h_ffs_server h_quota2_tests
SRCS.h_ffs_server=	h_ffs_server.c
MAN.h_ffs_server=	# empty
BINDIR.h_ffs_server=	${TESTSDIR}

SRCS.h_quota2_tests=	h_quota2_tests.c
MAN.h_quota2_tests=	# empty
BINDIR.h_quota2_tests=	${TESTSDIR}

.for name in t_getquota t_setquota t_quotalimit t_clearquota t_miscquota
TESTS_SH+=	${name}
TESTS_SH_SRC_${name}=	ffs_common.sh quotas_common.sh ${name}.sh
.endfor

TESTS_C+=	t_fifos
TESTS_C+=	t_snapshot
TESTS_C+=	t_snapshot_log
TESTS_C+=	t_snapshot_v2
TESTS_C+=	t_mount
TESTS_C+=	t_quota2_1
TESTS_C+=	t_quota2_remount

LDADD+=-lrumpfs_ffs						# ffs
LDADD+=-lrumpdev_fss						# snapshot dev
LDADD+=-lrumpdev_disk -lrumpdev					# disk device
LDADD+=-lrumpvfs_fifofs -lrumpnet_local -lrumpnet_net -lrumpnet	# fifos
VFSTESTDIR != cd ${.CURDIR}/../common && ${PRINTOBJDIR}
LDADD+=-L${VFSTESTDIR} -lvfstest
LDADD+=-lrumpvfs -lrumpkern_sysproxy -lrump -lrumpuser		# base
LDADD+=-lpthread

.include <bsd.test.mk>
