.PATH: ${ZFSTOP}/lib/libuutil

PACKAGE=	zfs
LIB_PACKAGE=

LIB=	uutil
SRCS=\
	uu_alloc.c \
	uu_avl.c \
	uu_ident.c \
	uu_list.c \
	uu_misc.c \
	uu_string.c

WARNS?=	2
CFLAGS+= -DIN_BASE
CFLAGS+= -I${ZFSTOP}/include
CFLAGS+= -I${ZFSTOP}/lib/libspl/include/
CFLAGS+= -I${ZFSTOP}/lib/libspl/include/os/freebsd
CFLAGS+= -I${SRCTOP}/sys
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -include ${ZFSTOP}/include/os/freebsd/spl/sys/ccompile.h

LIBADD= avl spl

.include <bsd.lib.mk>
