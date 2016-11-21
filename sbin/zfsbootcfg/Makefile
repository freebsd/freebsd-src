#	@(#)Makefile	8.4 (Berkeley) 6/22/95
# $FreeBSD$

PROG=	zfsbootcfg
WARNS?=	1
MAN=	zfsbootcfg.8

DPADD+=${LIBZFS}
DPADD+=${LIBNVPAIR}
DPADD+=${LIBUMEM}
DPADD+=${LIBUUTIL}
DPADD+=${LIBGEOM}

LDADD+=-lzfs
LDADD+=-lnvpair
LDADD+=-lumem
LDADD+=-luutil
LDADD+=-lgeom

CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/lib/libumem
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libzfs/common
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libzfs_core/common
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libzpool/common
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libnvpair
CFLAGS+= -I${SRCTOP}/sys/cddl/compat/opensolaris
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/head

CFLAGS+= -DNEED_SOLARIS_BOOLEAN

.include <bsd.prog.mk>
