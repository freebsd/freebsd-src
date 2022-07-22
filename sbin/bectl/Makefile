# $FreeBSD$

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
	zfsbootenv

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

HAS_TESTS=	yes
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
