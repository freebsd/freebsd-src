#	$NetBSD: Makefile,v 1.1 2010/04/13 10:21:47 pooka Exp $
#

TESTSDIR=	${TESTSBASE}/fs/msdosfs
WARNS=		4

TESTS_C=	t_snapshot

LDADD+=-lrumpfs_msdos -lrumpfs_tmpfs				# fs drivers
LDADD+=-lrumpdev_fss						# snapshot dev
LDADD+=-lrumpdev_disk -lrumpdev					# disk device
LDADD+=-lrumpvfs -lrump -lrumpuser -lpthread			# base

.include <bsd.test.mk>
