#	From: @(#)Makefile	5.15 (Berkeley) 6/29/90
#
#	$Id: Makefile,v 1.5 1994/02/04 03:03:03 wollman Exp $

PROG=	fsck
MAN8=	fsck.8
SRCS=	dir.c inode.c main.c pass1.c pass1b.c pass2.c pass3.c pass4.c \
	pass5.c preen.c setup.c utilities.c ufs_subr.c ufs_tables.c
.PATH:	${.CURDIR}/../../sys/ufs
LDADD+= -lutil

.include <bsd.prog.mk>
