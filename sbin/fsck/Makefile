#	@(#)Makefile	8.2 (Berkeley) 4/27/95

PROG=	fsck
MAN8=	fsck.0
SRCS=	dir.c inode.c main.c pass1.c pass1b.c pass2.c pass3.c pass4.c \
	pass5.c preen.c setup.c utilities.c ffs_subr.c ffs_tables.c
CFLAGS+=-W
.PATH:	${.CURDIR}/../../sys/ufs/ffs

.include <bsd.prog.mk>
