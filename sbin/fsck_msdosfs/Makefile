#	$NetBSD: Makefile,v 1.6 1997/05/08 21:11:11 gwr Exp $
# $FreeBSD: src/sbin/fsck_msdosfs/Makefile,v 1.3 2001/12/04 02:19:47 obrien Exp $

FSCK=	${.CURDIR}/../fsck
.PATH: ${FSCK}

PROG=	fsck_msdosfs
MAN=	fsck_msdosfs.8
SRCS=	main.c check.c boot.c fat.c dir.c fsutil.c

CFLAGS+= -I${FSCK}
WARNS=	0

.include <bsd.prog.mk>
