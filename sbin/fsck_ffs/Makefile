# $FreeBSD$
#	@(#)Makefile	8.2 (Berkeley) 4/27/95

PROG=	fsck_ffs
LINKS+=	${BINDIR}/fsck_ffs ${BINDIR}/fsck_ufs
LINKS+=	${BINDIR}/fsck_ffs ${BINDIR}/fsck_4.2bsd
MAN=	fsck_ffs.8
MLINKS=	fsck_ffs.8 fsck_ufs.8 fsck_ffs.8 fsck_4.2bsd.8
SRCS=	dir.c ea.c fsutil.c inode.c main.c pass1.c pass1b.c pass2.c pass3.c \
	pass4.c pass5.c setup.c suj.c utilities.c gjournal.c getmntopts.c \
	globs.c
DPADD=	${LIBUFS}
LDADD=	-lufs
WARNS?=	2
CFLAGS+= -I${.CURDIR} -I${.CURDIR}/../mount

.PATH:	${.CURDIR}/../../sys/ufs/ffs ${.CURDIR}/../mount

.include <bsd.prog.mk>
