# $FreeBSD$

LIB=	archive
SHLIB_MAJOR=	1
VERSION=	1.00.002
CFLAGS+=	-DPACKAGE_NAME=\"lib${LIB}\"
CFLAGS+=	-DPACKAGE_VERSION=\"${VERSION}\"

.if ${MACHINE_ARCH} == "arm"
WARNS?= 3
.else
WARNS?=	6
.endif

# I'm not yet ready for a shared version of this library, as
# there are a couple of API changes still in the works.
NOPIC=	1

INCS=	archive.h archive_entry.h

SRCS=	archive_check_magic.c				\
	archive_entry.c					\
	archive_read.c					\
	archive_read_data_into_buffer.c			\
	archive_read_data_into_fd.c			\
	archive_read_extract.c				\
	archive_read_open_fd.c				\
	archive_read_open_file.c			\
	archive_read_support_compression_all.c		\
	archive_read_support_compression_bzip2.c	\
	archive_read_support_compression_compress.c	\
	archive_read_support_compression_gzip.c		\
	archive_read_support_compression_none.c		\
	archive_read_support_format_all.c		\
	archive_read_support_format_cpio.c		\
	archive_read_support_format_tar.c		\
	archive_string.c				\
	archive_string_sprintf.c			\
	archive_util.c					\
	archive_write.c					\
	archive_write_open_fd.c				\
	archive_write_open_file.c			\
	archive_write_set_compression_bzip2.c		\
	archive_write_set_compression_gzip.c		\
	archive_write_set_compression_none.c		\
	archive_write_set_format.c			\
	archive_write_set_format_by_name.c		\
	archive_write_set_format_cpio.c			\
	archive_write_set_format_pax.c			\
	archive_write_set_format_shar.c			\
	archive_write_set_format_ustar.c

MAN=	archive_entry.3					\
	archive_read.3					\
	archive_util.3					\
	archive_write.3					\
	libarchive.3					\
	libarchive-formats.5				\
	tar.5

MLINKS+=	archive_entry.3 archive_entry_acl_add_entry.3
MLINKS+=	archive_entry.3 archive_entry_acl_add_entry_w.3
MLINKS+=	archive_entry.3 archive_entry_acl_clear.3
MLINKS+=	archive_entry.3 archive_entry_acl_count.3
MLINKS+=	archive_entry.3 archive_entry_acl_next.3
MLINKS+=	archive_entry.3 archive_entry_acl_next_w.3
MLINKS+=	archive_entry.3 archive_entry_acl_reset.3
MLINKS+=	archive_entry.3 archive_entry_acl_text_w.3
MLINKS+=	archive_entry.3 archive_entry_clear.3
MLINKS+=	archive_entry.3 archive_entry_clone.3
MLINKS+=	archive_entry.3 archive_entry_copy_fflags_text_w.3
MLINKS+=	archive_entry.3 archive_entry_copy_gname_w.3
MLINKS+=	archive_entry.3 archive_entry_copy_hardlink_w.3
MLINKS+=	archive_entry.3 archive_entry_copy_pathname_w.3
MLINKS+=	archive_entry.3 archive_entry_copy_stat.3
MLINKS+=	archive_entry.3 archive_entry_copy_symlink_w.3
MLINKS+=	archive_entry.3 archive_entry_copy_uname_w.3
MLINKS+=	archive_entry.3 archive_entry_fflags.3
MLINKS+=	archive_entry.3 archive_entry_fflags_text.3
MLINKS+=	archive_entry.3 archive_entry_free.3
MLINKS+=	archive_entry.3 archive_entry_gid.3
MLINKS+=	archive_entry.3 archive_entry_gname.3
MLINKS+=	archive_entry.3 archive_entry_gname_w.3
MLINKS+=	archive_entry.3 archive_entry_hardlink.3
MLINKS+=	archive_entry.3 archive_entry_ino.3
MLINKS+=	archive_entry.3 archive_entry_mode.3
MLINKS+=	archive_entry.3 archive_entry_mtime.3
MLINKS+=	archive_entry.3 archive_entry_mtime_nsec.3
MLINKS+=	archive_entry.3 archive_entry_new.3
MLINKS+=	archive_entry.3 archive_entry_pathname.3
MLINKS+=	archive_entry.3 archive_entry_pathname_w.3
MLINKS+=	archive_entry.3 archive_entry_rdev.3
MLINKS+=	archive_entry.3 archive_entry_rdevmajor.3
MLINKS+=	archive_entry.3 archive_entry_rdevminor.3
MLINKS+=	archive_entry.3 archive_entry_set_fflags.3
MLINKS+=	archive_entry.3 archive_entry_set_gid.3
MLINKS+=	archive_entry.3 archive_entry_set_gname.3
MLINKS+=	archive_entry.3 archive_entry_set_hardlink.3
MLINKS+=	archive_entry.3 archive_entry_set_link.3
MLINKS+=	archive_entry.3 archive_entry_set_mode.3
MLINKS+=	archive_entry.3 archive_entry_set_pathname.3
MLINKS+=	archive_entry.3 archive_entry_set_rdevmajor.3
MLINKS+=	archive_entry.3 archive_entry_set_rdevminor.3
MLINKS+=	archive_entry.3 archive_entry_set_size.3
MLINKS+=	archive_entry.3 archive_entry_set_symlink.3
MLINKS+=	archive_entry.3 archive_entry_set_uid.3
MLINKS+=	archive_entry.3 archive_entry_set_uname.3
MLINKS+=	archive_entry.3 archive_entry_size.3
MLINKS+=	archive_entry.3 archive_entry_stat.3
MLINKS+=	archive_entry.3 archive_entry_symlink.3
MLINKS+=	archive_entry.3 archive_entry_uid.3
MLINKS+=	archive_entry.3 archive_entry_uname.3
MLINKS+=	archive_entry.3 archive_entry_uname_w.3
MLINKS+=	archive_read.3 archive_read_data.3
MLINKS+=	archive_read.3 archive_read_data_block.3
MLINKS+=	archive_read.3 archive_read_data_into_buffer.3
MLINKS+=	archive_read.3 archive_read_data_into_fd.3
MLINKS+=	archive_read.3 archive_read_data_skip.3
MLINKS+=	archive_read.3 archive_read_extract.3
MLINKS+=	archive_read.3 archive_read_extract_set_progress_callback.3
MLINKS+=	archive_read.3 archive_read_finish.3
MLINKS+=	archive_read.3 archive_read_new.3
MLINKS+=	archive_read.3 archive_read_next_header.3
MLINKS+=	archive_read.3 archive_read_open.3
MLINKS+=	archive_read.3 archive_read_open_fd.3
MLINKS+=	archive_read.3 archive_read_open_file.3
MLINKS+=	archive_read.3 archive_read_set_bytes_per_block.3
MLINKS+=	archive_read.3 archive_read_support_compression_all.3
MLINKS+=	archive_read.3 archive_read_support_compression_bzip2.3
MLINKS+=	archive_read.3 archive_read_support_compression_compress.3
MLINKS+=	archive_read.3 archive_read_support_compression_gzip.3
MLINKS+=	archive_read.3 archive_read_support_compression_none.3
MLINKS+=	archive_read.3 archive_read_support_format_all.3
MLINKS+=	archive_read.3 archive_read_support_format_cpio.3
MLINKS+=	archive_read.3 archive_read_support_format_tar.3
MLINKS+=	archive_util.3 archive_compression.3
MLINKS+=	archive_util.3 archive_compression_name.3
MLINKS+=	archive_util.3 archive_errno.3
MLINKS+=	archive_util.3 archive_error_string.3
MLINKS+=	archive_util.3 archive_format.3
MLINKS+=	archive_util.3 archive_format_name.3
MLINKS+=	archive_write.3 archive_write_data.3
MLINKS+=	archive_write.3 archive_write_finish.3
MLINKS+=	archive_write.3 archive_write_header.3
MLINKS+=	archive_write.3 archive_write_new.3
MLINKS+=	archive_write.3 archive_write_open.3
MLINKS+=	archive_write.3 archive_write_open_fd.3
MLINKS+=	archive_write.3 archive_write_open_file.3
MLINKS+=	archive_write.3 archive_write_prepare.3
MLINKS+=	archive_write.3 archive_write_set_bytes_per_block.3
MLINKS+=	archive_write.3 archive_write_set_bytes_in_last_block.3
MLINKS+=	archive_write.3 archive_write_set_callbacks.3
MLINKS+=	archive_write.3 archive_write_set_compression_bzip2.3
MLINKS+=	archive_write.3 archive_write_set_compression_gzip.3
MLINKS+=	archive_write.3 archive_write_set_format_pax.3
MLINKS+=	archive_write.3 archive_write_set_format_shar.3
MLINKS+=	archive_write.3 archive_write_set_format_ustar.3
MLINKS+=	libarchive.3 archive.3

.include <bsd.lib.mk>
