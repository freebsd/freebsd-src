#	$NetBSD: Makefile,v 1.4 2010/07/13 21:13:23 jmmv Exp $

TESTSDIR=       ${TESTSBASE}/fs/puffs/h_dtfs
ATFFILE=	no

TESTS_C=	h_dtfs

SRCS.h_dtfs=	dtfs.c dtfs_vfsops.c dtfs_vnops.c dtfs_subr.c
LDADD+=		-lpuffs -lutil

WARNS=2

.include <bsd.test.mk>
