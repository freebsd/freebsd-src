.PATH: ${SRCTOP}/sys/compat/lindebugfs

KMOD=	lindebugfs
SRCS=	lindebugfs.c
SRCS+=	${LINUXKPI_GENSRCS}

CFLAGS+= ${LINUXKPI_INCLUDES}

EXPORT_SYMS=	debugfs_create_atomic_t
EXPORT_SYMS+=	debugfs_create_blob
EXPORT_SYMS+=	debugfs_create_bool
EXPORT_SYMS+=	debugfs_create_dir
EXPORT_SYMS+=	debugfs_create_file
EXPORT_SYMS+=	debugfs_create_file_size
EXPORT_SYMS+=	debugfs_create_file_unsafe
EXPORT_SYMS+=	debugfs_create_mode_unsafe
EXPORT_SYMS+=	debugfs_create_str
EXPORT_SYMS+=	debugfs_create_symlink
EXPORT_SYMS+=	debugfs_create_u8
EXPORT_SYMS+=	debugfs_create_u16
EXPORT_SYMS+=	debugfs_create_u32
EXPORT_SYMS+=	debugfs_create_u64
EXPORT_SYMS+=	debugfs_create_ulong
EXPORT_SYMS+=	debugfs_create_x8
EXPORT_SYMS+=	debugfs_create_x16
EXPORT_SYMS+=	debugfs_create_x32
EXPORT_SYMS+=	debugfs_create_x64
EXPORT_SYMS+=	debugfs_lookup
EXPORT_SYMS+=	debugfs_remove
EXPORT_SYMS+=	debugfs_remove_recursive

.include <bsd.kmod.mk>
