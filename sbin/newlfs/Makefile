#	@(#)Makefile	8.1 (Berkeley) 6/18/93

PROG=	newlfs
CFLAGS+=-I${.CURDIR}/../../sys/ufs/lfs
SRCS=	dkcksum.c lfs.c lfs_cksum.c misc.c newfs.c
MAN8=	newlfs.8
.PATH:	${.CURDIR}/../../sys/ufs/lfs ${.CURDIR}/../disklabel

.include <bsd.prog.mk>
