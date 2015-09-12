# $FreeBSD$

.include <src.opts.mk>

PROG=	fstyp
SRCS=	cd9660.c ext2fs.c fstyp.c geli.c msdosfs.c ntfs.c ufs.c

.if ${MK_ZFS} != "no"
SRCS +=	zfs.c
.endif

MAN=	fstyp.8

WARNS?=	0

.if ${MK_ZFS} != "no"
IGNORE_PRAGMA=  YES

CFLAGS+= -DNEED_SOLARIS_BOOLEAN -DHAVE_ZFS
CFLAGS+= -I${.CURDIR}/../../sys/cddl/compat/opensolaris
CFLAGS+= -I${.CURDIR}/../../cddl/compat/opensolaris/include
CFLAGS+= -I${.CURDIR}/../../cddl/compat/opensolaris/lib/libumem
CFLAGS+= -I${.CURDIR}/../../cddl/contrib/opensolaris/lib/libnvpair
CFLAGS+= -I${.CURDIR}/../../cddl/contrib/opensolaris/lib/libzpool/common
CFLAGS+= -I${.CURDIR}/../../sys/cddl/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+= -I${.CURDIR}/../../sys/cddl/contrib/opensolaris/uts/common
CFLAGS+= -I${.CURDIR}/../../sys/cddl/contrib/opensolaris/uts/common/sys
CFLAGS+= -I${.CURDIR}/../../cddl/contrib/opensolaris/head
.endif

CFLAGS+=-I${.CURDIR}/../../sys

DPADD=   ${LIBGEOM} ${LIBMD}
LDADD=   -lgeom -lmd

.if ${MK_ZFS} != "no"
DPADD += ${LIBNVPAIR} ${LIBZFS}
LDADD += -lnvpair -lzfs
.endif

.include <bsd.prog.mk>
