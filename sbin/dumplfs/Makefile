#	@(#)Makefile	8.1 (Berkeley) 6/18/93

PROG=	dumplfs
CFLAGS+=-I${.CURDIR}/../../sys/ufs/lfs
SRCS=	dumplfs.c lfs_cksum.c misc.c
.PATH:	${.CURDIR}/../../sys/ufs/lfs
MAN8=	dumplfs.8

.include <bsd.prog.mk>
