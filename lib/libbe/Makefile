
SHLIBDIR?=	/lib

.include <src.opts.mk>

PACKAGE=	zfs
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

LIBADD+= zfs
LIBADD+= nvpair
LIBADD+= spl
LIBADD+= zfsbootenv

CFLAGS+= -DIN_BASE -DHAVE_RPC_TYPES
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/include/os/freebsd
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libspl/include/os/freebsd
CFLAGS+= -I${SRCTOP}/sys/contrib/openzfs/lib/libzfs
CFLAGS+= -I${SRCTOP}/sys
CFLAGS+= -I${SRCTOP}/cddl/compat/opensolaris/include
CFLAGS+= -include ${SRCTOP}/sys/contrib/openzfs/include/os/freebsd/spl/sys/ccompile.h
CFLAGS+= -DHAVE_ISSETUGID
CFLAGS.be.c=		-Wno-cast-qual
CFLAGS.be_access.c=	-Wno-cast-qual
CFLAGS.be_error.c=	-Wno-cast-qual
CFLAGS.be_info.c=	-Wno-cast-qual

HAS_TESTS=	YES
SUBDIR.${MK_TESTS}+= tests

.include <bsd.lib.mk>
