# $FreeBSD$

.PATH: ${SRCTOP}/sys/contrib/openzfs/lib/libtpool
.PATH: ${SRCTOP}/sys/contrib/openzfs/include


LIB=	tpool
LIBADD=	spl
PACKAGE=	runtime

INCS=	thread_pool_impl.h
SRCS=	thread_pool.c

WARNS?=	2
CSTD=	c99
CFLAGS+= -DIN_BASE
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/os/freebsd
CFLAGS+= -I${SRCTOP}/sys
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/module/icp/include
CFLAGS+= -include ${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/spl/sys/ccompile.h
CFLAGS+= -DHAVE_ISSETUGID 
CFLAGS+= -include ${SRCTOP}/sys/modules/zfs/zfs_config.h

.include <bsd.lib.mk>
