# $FreeBSD$

ZFSTOP=	${SRCTOP}/sys/contrib/openzfs

.PATH: ${ZFSTOP}/man/man4
.PATH: ${ZFSTOP}/man/man5
.PATH: ${ZFSTOP}/man/man7
.PATH: ${ZFSTOP}/man/man8
.PATH: ${ZFSTOP}/cmd/zpool
.PATH: ${ZFSTOP}/cmd/zpool/os/freebsd


PACKAGE=	zfs
PROG=	zpool
MAN= \
	spl.4 \
	zfs.4 \
	zpool.8 \
	zpool-add.8 \
	zpool-attach.8 \
	zpool-checkpoint.8 \
	zpool-clear.8 \
	zpool-create.8 \
	zpool-destroy.8 \
	zpool-detach.8 \
	zpool-events.8 \
	zpool-export.8 \
	zpool-features.7 \
	zpool-get.8 \
	zpool-history.8 \
	zpool-import.8 \
	zpool-initialize.8 \
	zpool-iostat.8 \
	zpool-labelclear.8 \
	zpool-list.8 \
	zpool-offline.8 \
	zpool-reguid.8 \
	zpool-remove.8 \
	zpool-reopen.8 \
	zpool-replace.8 \
	zpool-resilver.8 \
	zpool-scrub.8 \
	zpool-split.8 \
	zpool-status.8 \
	zpool-sync.8 \
	zpool-trim.8 \
	zpool-upgrade.8 \
	zpool-wait.8 \
	zpoolconcepts.7 \
	zpoolprops.7
MLINKS= \
	zpool-offline.8 zpool-online.8 \
	zpool-get.8 zpool-set.8
SRCS= \
	zpool_iter.c \
	zpool_main.c \
	zpool_util.c \
	zpool_util.h \
	zpool_vdev.c \
	zpool_vdev_os.c

WARNS?=	2

CFLAGS+= \
	-DIN_BASE \
	-I${ZFSTOP}/include \
	-I${ZFSTOP}/lib/libspl/include \
	-I${ZFSTOP}/lib/libspl/include/os/freebsd \
	-I${SRCTOP}/sys \
	-I${SRCTOP}/cddl/compat/opensolaris/include \
	-I${ZFSTOP}/cmd/zpool \
	-include ${ZFSTOP}/include/os/freebsd/spl/sys/ccompile.h \
	-DHAVE_ISSETUGID \
	-include ${SRCTOP}/sys/modules/zfs/zfs_config.h \
	-DSYSCONFDIR=\"/etc\" \
	-DPKGDATADIR=\"/usr/share/zfs\"

LIBADD=	geom nvpair uutil zfs zutil avl spl tpool zfs_core m
LDADD+= 	-pthread
.include <bsd.prog.mk>
