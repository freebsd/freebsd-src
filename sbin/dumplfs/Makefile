#	@(#)Makefile	8.1 (Berkeley) 6/18/93

PROG=	dumplfs
CFLAGS+=-I/sys/ufs/lfs
SRCS=	dumplfs.c lfs_cksum.c misc.c
.PATH:	/sys/ufs/lfs
MAN8=	dumplfs.0

.include <bsd.prog.mk>
