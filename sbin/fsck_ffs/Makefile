# $FreeBSD$
#	@(#)Makefile	8.2 (Berkeley) 4/27/95

PROG=	fsck_ffs
LINKS+=	${BINDIR}/fsck_ffs ${BINDIR}/fsck_ufs
LINKS+=	${BINDIR}/fsck_ffs ${BINDIR}/fsck_4.2bsd
MAN=	fsck_ffs.8
SRCS=	dir.c fsutil.c inode.c main.c pass1.c pass1b.c pass2.c pass3.c pass4.c \
	pass5.c setup.c utilities.c ffs_subr.c ffs_tables.c
WARNS=	2
CFLAGS+= -I${.CURDIR}

.PATH:	${.CURDIR}/../../sys/ufs/ffs

.include <bsd.prog.mk>
