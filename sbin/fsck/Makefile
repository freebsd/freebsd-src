#	@(#)Makefile	5.15 (Berkeley) 6/29/90
#
#	$Header: /a/cvs/386BSD/src/sbin/fsck/Makefile,v 1.4 1993/07/26 22:47:35 nate Exp $

PROG=	fsck
MAN8=	fsck.8
SRCS=	dir.c inode.c main.c pass1.c pass1b.c pass2.c pass3.c pass4.c \
	pass5.c preen.c setup.c utilities.c ufs_subr.c ufs_tables.c
.PATH:	${.CURDIR}/../../sys/ufs

.include <bsd.prog.mk>
