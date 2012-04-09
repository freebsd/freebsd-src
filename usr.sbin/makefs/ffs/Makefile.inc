#	$FreeBSD: src/usr.sbin/makefs/ffs/Makefile.inc,v 1.1.2.2.2.1 2012/03/03 06:15:13 kensmith Exp $
#

.PATH:	${.CURDIR}/ffs ${.CURDIR}/../../sys/ufs/ffs

CFLAGS+=	-I${.CURDIR}/../../sys/ufs/ffs

SRCS+=	ffs_alloc.c ffs_balloc.c ffs_bswap.c ffs_subr.c ufs_bmap.c
SRCS+=	buf.c mkfs.c
