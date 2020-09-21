# $FreeBSD$

SHLIBDIR?=	/lib

.include <src.opts.mk>

LIB=		be
SHLIB_MAJOR=	1
SHLIB_MINOR=	0

SRCS=		be.c be_access.c be_error.c be_info.c
INCS=		be.h
MAN=		libbe.3

LIBADD+= zfs
LIBADD+= nvpair
LIBADD+= spl
LIBADD+= zfsbootenv

CFLAGS+= -DIN_BASE -DHAVE_RPC_TYPES
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include/os/freebsd
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/os/freebsd
CFLAGS+= -I${SRCTOP}/sys
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -include ${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/spl/sys/ccompile.h
CFLAGS+= -DHAVE_ISSETUGID


HAS_TESTS=	YES
SUBDIR.${MK_TESTS}+= tests

.include <bsd.lib.mk>
