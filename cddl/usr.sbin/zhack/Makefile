# $FreeBSD$

.PATH: ${SRCTOP}/cddl/contrib/opensolaris/cmd/zhack

PROG=	zhack
MAN=

WARNS?=	0
CSTD=	c99

CFLAGS+= -I${SRCTOP}/sys/cddl/compat/opensolaris
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/lib/libumem
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libnvpair
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libuutil/common
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libzfs/common
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libzfs_core/common
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libzpool/common
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common/sys
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/common/zfs
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/head

DPADD=	${LIBGEOM} ${LIBM} ${LIBNVPAIR} ${LIBPTHREAD} ${LIBUMEM} \
	${LIBUUTIL} ${LIBZFS_CORE} ${LIBZFS} ${LIBZPOOL}
LDADD=	-lgeom -lm -lnvpair -lpthread -lumem -luutil -lzfs_core -lzfs -lzpool

CFLAGS+=	-DDEBUG=1
#DEBUG_FLAGS+=	-g

.include <bsd.prog.mk>
