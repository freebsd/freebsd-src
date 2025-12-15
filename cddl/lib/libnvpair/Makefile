.PATH: ${ZFSTOP}/module/nvpair
.PATH: ${ZFSTOP}/lib/libnvpair
.PATH: ${ZFSTOP}/include

PACKAGE=	zfs
LIB_PACKAGE=

LIB=	nvpair
LIBADD=	spl

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
CFLAGS+= -I${ZFSTOP}/include
CFLAGS+= -I${ZFSTOP}/lib/libspl/include/
CFLAGS+= -I${ZFSTOP}/lib/libspl/include/os/freebsd
CFLAGS+= -I${SRCTOP}/sys
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -include ${ZFSTOP}/include/os/freebsd/spl/sys/ccompile.h
CFLAGS+= -DHAVE_ISSETUGID  -DHAVE_CONFIG_H -DHAVE_XDR_BYTESREC

.include <bsd.lib.mk>
