# $FreeBSD$

SHLIBDIR?=	/lib

.include <src.opts.mk>

PACKAGE=	lib${LIB}
LIB=		be
SHLIB_MAJOR=	1
SHLIB_MINOR=	0

SRCS=		be.c be_access.c be_error.c be_info.c
INCS=		be.h
MAN=		libbe.3
MLINKS+=	libbe.3 be_activate.3
MLINKS+=	libbe.3 be_active_name.3
MLINKS+=	libbe.3 be_active_path.3
MLINKS+=	libbe.3 be_create_depth.3
MLINKS+=	libbe.3 be_create_from_existing_snap.3
MLINKS+=	libbe.3 be_create_from_existing.3
MLINKS+=	libbe.3 be_create.3
MLINKS+=	libbe.3 be_deactivate.3
MLINKS+=	libbe.3 be_destroy.3
MLINKS+=	libbe.3 be_exists.3
MLINKS+=	libbe.3 be_export.3
MLINKS+=	libbe.3 be_get_bootenv_props.3
MLINKS+=	libbe.3 be_get_dataset_props.3
MLINKS+=	libbe.3 be_get_dataset_snapshots.3
MLINKS+=	libbe.3 be_import.3
MLINKS+=	libbe.3 be_is_auto_snapshot_name.3
MLINKS+=	libbe.3 be_mount.3
MLINKS+=	libbe.3 be_mounted_at.3
MLINKS+=	libbe.3 be_nextboot_name.3
MLINKS+=	libbe.3 be_nextboot_path.3
MLINKS+=	libbe.3 be_nicenum.3
MLINKS+=	libbe.3 be_prop_list_alloc.3
MLINKS+=	libbe.3 be_prop_list_free.3
MLINKS+=	libbe.3 be_rename.3
MLINKS+=	libbe.3 be_root_concat.3
MLINKS+=	libbe.3 be_root_path.3
MLINKS+=	libbe.3 be_snapshot.3
MLINKS+=	libbe.3 be_unmount.3
MLINKS+=	libbe.3 be_validate_name.3
MLINKS+=	libbe.3 be_validate_snap.3
MLINKS+=	libbe.3 libbe_close.3
MLINKS+=	libbe.3 libbe_errno.3
MLINKS+=	libbe.3 libbe_error_description.3
MLINKS+=	libbe.3 libbe_init.3
MLINKS+=	libbe.3 libbe_print_on_error.3

WARNS?=	2
IGNORE_PRAGMA=	yes

LIBADD+= zfs
LIBADD+= nvpair

CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libzfs/common
CFLAGS+= -I${SRCTOP}/sys/cddl/compat/opensolaris
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/lib/libumem
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libzpool/common
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/common/zfs
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common/fs/zfs
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common
CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/head

CFLAGS+= -DNEED_SOLARIS_BOOLEAN

HAS_TESTS=	YES
SUBDIR.${MK_TESTS}+= tests

.include <bsd.lib.mk>
