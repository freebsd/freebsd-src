#	$NetBSD: Makefile,v 1.6 1997/05/08 21:11:11 gwr Exp $
# $FreeBSD: src/sbin/fsck_msdosfs/Makefile,v 1.5.34.1 2010/12/21 17:10:29 kensmith Exp $

FSCK=	${.CURDIR}/../fsck
.PATH: ${FSCK}

PROG=	fsck_msdosfs
MAN=	fsck_msdosfs.8
SRCS=	main.c check.c boot.c fat.c dir.c fsutil.c

CFLAGS+= -I${FSCK}
WARNS?=	0

.include <bsd.prog.mk>
