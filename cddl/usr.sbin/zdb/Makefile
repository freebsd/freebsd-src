.PATH: ${ZFSTOP}/cmd/zdb
.PATH: ${ZFSTOP}/man/man8

PACKAGE=	zfs
PROG=	zdb
MAN=	zdb.8
INCS=	zdb.h
SRCS=	zdb.c zdb_il.c

WARNS?=	2
CSTD=	c99

CFLAGS+= \
	-DIN_BASE \
	-I${ZFSTOP}/include \
	-I${ZFSTOP}/lib/libspl/include \
	-I${ZFSTOP}/lib/libspl/include/os/freebsd \
	-I${ZFSTOP}/lib/libspl/include/os/freebsd/spl \
	-I${ZFSTOP}/lib/libzpool/include \
	-I${SRCTOP}/sys \
	-include ${ZFSTOP}/include/os/freebsd/spl/sys/ccompile.h \
	-DHAVE_ISSETUGID

LIBADD=	nvpair umem zdb zfs_core zfs spl avl zutil zpool crypto pthread

CFLAGS.gcc+= -fms-extensions
# Since there are many asserts in this program, it makes no sense to compile
# it without debugging.
CFLAGS+=	-g -DDEBUG=1 -DZFS_DEBUG=1

.include <bsd.prog.mk>
