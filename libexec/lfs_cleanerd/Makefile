#	@(#)Makefile	8.1 (Berkeley) 6/5/93

PROG=	lfs_cleanerd
CFLAGS+=-I/sys/ufs/lfs -I${.CURDIR} -DDIAGNOSTIC
MAN8=	lfs_cleanerd.0
SRCS=	cleanerd.c lfs_cksum.c library.c misc.c print.c

.PATH:	/sys/ufs/lfs

.include <bsd.prog.mk>
