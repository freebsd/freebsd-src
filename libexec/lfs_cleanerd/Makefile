#	@(#)Makefile	8.1 (Berkeley) 6/5/93
#	$Id: Makefile,v 1.5.2.1 1997/08/25 12:47:27 jmg Exp $

PROG=	lfs_cleanerd
CFLAGS+=-I${.CURDIR}/../../sys/ufs/lfs -I${.CURDIR} ${DEBUG}
MAN8=	lfs_cleanerd.8
SRCS=	cleanerd.c lfs_cksum.c library.c misc.c print.c

.PATH:	${.CURDIR}/../../sys/ufs/lfs

.include <bsd.prog.mk>
