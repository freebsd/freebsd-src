#	@(#)Makefile	8.1 (Berkeley) 6/5/93
#	$Id: Makefile,v 1.7 1997/02/22 14:21:43 peter Exp $

PROG=	lfs_cleanerd
CFLAGS+=-I${.CURDIR}/../../sys/ufs/lfs -I${.CURDIR} ${DEBUG}
MAN8=	lfs_cleanerd.8
SRCS=	cleanerd.c lfs_cksum.c library.c misc.c print.c

.PATH:	${.CURDIR}/../../sys/ufs/lfs

.include <bsd.prog.mk>
