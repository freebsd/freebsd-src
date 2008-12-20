#	$FreeBSD$

PROG=	makefs
MAN=	makefs.8

WARNS?=	2

CFLAGS+=-I${.CURDIR}
SRCS=	ffs.c getid.c makefs.c walk.c

.PATH:	${.CURDIR}/ffs
CFLAGS+=-I${.CURDIR}/ffs
CFLAGS+=-DHAVE_STRUCT_STAT_ST_FLAGS=1
CFLAGS+=-DHAVE_STRUCT_STAT_ST_GEN=1
SRCS+=	buf.c ffs_alloc.c ffs_balloc.c ffs_bswap.c ffs_subr.c mkfs.c ufs_bmap.c

.PATH:	${.CURDIR}/compat
CFLAGS+=-I${.CURDIR}/compat
SRCS+=	pwcache.c strsuftoll.c

.PATH:	${.CURDIR}/../mtree
CFLAGS+=-I${.CURDIR}/../mtree
SRCS+=	misc.c spec.c

.PATH:	${.CURDIR}/../../sys/ufs/ffs
SRCS+=	ffs_tables.c

.include <bsd.prog.mk>
