# $FreeBSD$

.include <src.opts.mk>

ZFSTOP=	${SRCTOP}/sys/contrib/openzfs

.PATH: ${ZFSTOP}/cmd/ztest
.PATH: ${ZFSTOP}/man/man1

PROG=	ztest
MAN=	ztest.1

WARNS?=	2
CFLAGS+= \
	-DIN_BASE \
	-I${ZFSTOP}/include \
	-I${ZFSTOP}/lib/libspl/include \
	-I${ZFSTOP}/lib/libspl/include/os/freebsd \
	-I${SRCTOP}/sys \
	-I${SRCTOP}/cddl/compat/opensolaris/include \
	-I${ZFSTOP}/module/icp/include \
	-include ${ZFSTOP}/include/os/freebsd/spl/sys/ccompile.h \
	-DHAVE_ISSETUGID \
	-include ${SRCTOP}/sys/modules/zfs/zfs_config.h

LIBADD=	geom m nvpair umem zpool pthread avl zfs_core spl zutil zfs uutil icp

CSTD=	c99

# Since there are many asserts in this program, it makes no sense to compile
# it without debugging.
CFLAGS+= -g -DDEBUG=1 -Wno-format -DZFS_DEBUG=1
CFLAGS.gcc+= -fms-extensions

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
