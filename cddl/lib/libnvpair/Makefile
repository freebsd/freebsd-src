
.PATH: ${SRCTOP}/sys/contrib/openzfs/module/nvpair
.PATH: ${SRCTOP}/sys/contrib/openzfs/lib/libnvpair
.PATH: ${SRCTOP}/sys/contrib/openzfs/include

LIB=	nvpair
LIBADD=	spl

PACKAGE=	zfs
# user
INCS=	libnvpair.h
SRCS=	libnvpair.c \
	libnvpair_json.c \
	nvpair_alloc_system.c
# kernel
SRCS+= nvpair_alloc_fixed.c \
	nvpair.c \
	fnvpair.c

WARNS?=	2
CFLAGS+= -DIN_BASE -DHAVE_RPC_TYPES
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/os/freebsd
CFLAGS+= -I${SRCTOP}/sys
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -include ${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/spl/sys/ccompile.h
CFLAGS+= -DHAVE_ISSETUGID  -DHAVE_CONFIG_H -DHAVE_XDR_BYTESREC


CFLAGS.nvpair.c+= -UHAVE_RPC_TYPES
.include <bsd.lib.mk>
