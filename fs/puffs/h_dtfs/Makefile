#	$NetBSD: Makefile,v 1.5 2016/01/23 21:22:48 christos Exp $

TESTSDIR=       ${TESTSBASE}/fs/puffs/h_dtfs
ATFFILE=	no

TESTS_C=	h_dtfs

CPPFLAGS+=	-D_KERNTYPES
SRCS.h_dtfs=	dtfs.c dtfs_vfsops.c dtfs_vnops.c dtfs_subr.c
LDADD+=		-lpuffs -lutil

WARNS=2

.include <bsd.test.mk>
