
ZFSTOP=	${SRCTOP}/sys/contrib/openzfs

.PATH: ${ZFSTOP}/cmd/zfs
.PATH: ${ZFSTOP}/man/man7
.PATH: ${ZFSTOP}/man/man8
.PATH: ${ZFSTOP}/module/os/freebsd/spl

PACKAGE=	zfs
PROG=	zfs
MAN= \
	zfs.8 \
	zfs-allow.8 \
	zfs-bookmark.8 \
	zfs-clone.8 \
	zfs-create.8 \
	zfs-destroy.8 \
	zfs-diff.8 \
	zfs-hold.8 \
	zfs-jail.8 \
	zfs-list.8 \
	zfs-load-key.8 \
	zfs-mount.8 \
	zfs-program.8 \
	zfs-project.8 \
	zfs-promote.8 \
	zfs-receive.8 \
	zfs-rename.8 \
	zfs-rollback.8 \
	zfs-send.8 \
	zfs-set.8 \
	zfs-share.8 \
	zfs-snapshot.8 \
	zfs-upgrade.8 \
	zfs-userspace.8 \
	zfs-wait.8 \
	zfsconcepts.7 \
	zfsprops.7
MLINKS= \
	zfs-allow.8 zfs-unallow.8 \
	zfs-hold.8 zfs-release.8 \
	zfs-jail.8 zfs-unjail.8 \
	zfs-load-key.8 zfs-change-key.8 \
	zfs-load-key.8 zfs-unload-key.8 \
	zfs-mount.8 zfs-unmount.8 \
	zfs-receive.8 zfs-recv.8 \
	zfs-send.8 zfs-redact.8 \
	zfs-set.8 zfs-get.8 \
	zfs-set.8 zfs-inherit.8 \
	zfs-userspace.8 zfs-groupspace.8 \
	zfs-userspace.8 zfs-projectspace.8
SRCS= \
	zfs_iter.c \
	zfs_iter.h \
	zfs_main.c \
	zfs_util.h \
	zfs_project.c \
	zfs_projectutil.h

WARNS?=	2

CFLAGS+= \
	-DIN_BASE \
	-I${ZFSTOP}/include \
	-I${ZFSTOP}/include/os/freebsd \
	-I${ZFSTOP}/lib/libspl/include \
	-I${ZFSTOP}/lib/libspl/include/os/freebsd \
	-I${SRCTOP}/sys \
	-I${SRCTOP}/cddl/compat/opensolaris/include \
	-I${ZFSTOP}/module/icp/include \
	-include ${ZFSTOP}/include/os/freebsd/spl/sys/ccompile.h \
	-DHAVE_ISSETUGID \
	-include ${SRCTOP}/sys/modules/zfs/zfs_config.h \
	-I${SRCTOP}/sys/modules/zfs

LIBADD=	jail avl nvpair geom  uutil zfs_core spl tpool zutil zfs m crypto
LDADD+=	-pthread
.include <bsd.prog.mk>

CWARNFLAGS.zfs_main.c+=	-Wno-unused-variable
