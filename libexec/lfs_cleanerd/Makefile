#	@(#)Makefile	8.1 (Berkeley) 6/5/93
#	$Id$

PROG=	lfs_cleanerd
CFLAGS+=-I/sys/ufs/lfs -I${.CURDIR} ${DEBUG}
MAN8=	lfs_cleanerd.8
SRCS=	cleanerd.c lfs_cksum.c library.c misc.c print.c

.PATH:	${.CURDIR}/../../sys/ufs/lfs

.include <bsd.prog.mk>
