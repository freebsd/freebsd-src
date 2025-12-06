.include <src.opts.mk>

PACKAGE=zfs
PROG=	bectl
MAN=	bectl.8

SRCS=	bectl.c bectl_jail.c bectl_list.c

LIBADD+= be \
	jail \
	nvpair \
	spl \
	util \
	zfsbootenv \
	pthread

CFLAGS+= -DIN_BASE
CFLAGS+= -I${ZFSTOP}/include
CFLAGS+= -I${ZFSTOP}/lib/libspl/include/
CFLAGS+= -I${ZFSTOP}/lib/libspl/include/os/freebsd
CFLAGS+= -I${SRCTOP}/sys
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -I${ZFSTOP}/module/icp/include
CFLAGS+= -include ${ZFSTOP}/include/os/freebsd/spl/sys/ccompile.h
CFLAGS+= -DHAVE_ISSETUGID -DHAVE_STRLCAT -DHAVE_STRLCPY
CFLAGS+= -include ${SRCTOP}/sys/modules/zfs/zfs_config.h

HAS_TESTS=	yes
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
