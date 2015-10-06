# $FreeBSD$

LIBARCHIVEDIR=	${SRCTOP}/contrib/libarchive

MAN=

PROG=libarchive_test
INTERNALPROG=yes  # Don't install this; it's just for testing
LDADD= -L ${.OBJDIR}/.. -larchive
LIBADD=	z bz2 lzma md crypto bsdxml
CFLAGS+= -g
CFLAGS+= -I${.CURDIR}/.. -I${.OBJDIR}
CFLAGS+= -I${LIBARCHIVEDIR}/libarchive -I${LIBARCHIVEDIR}/test_utils
CFLAGS+= -DHAVE_LIBLZMA=1 -DHAVE_LZMA_H=1

# Uncomment to link against dmalloc
#LDADD+= -L/usr/local/lib -ldmalloc
#CFLAGS+= -I/usr/local/include -DUSE_DMALLOC

.PATH:  ${LIBARCHIVEDIR}/libarchive/test
TESTS= \
	test_acl_freebsd_nfs4.c			\
	test_acl_freebsd_posix1e.c		\
	test_acl_nfs4.c				\
	test_acl_pax.c				\
	test_acl_posix1e.c			\
	test_archive_api_feature.c		\
	test_archive_clear_error.c		\
	test_archive_cmdline.c			\
	test_archive_crypto.c			\
	test_archive_getdate.c			\
	test_archive_match_time.c		\
	test_archive_match_owner.c		\
	test_archive_match_path.c		\
	test_archive_pathmatch.c		\
	test_archive_read_close_twice.c		\
	test_archive_read_close_twice_open_fd.c	\
	test_archive_read_close_twice_open_filename.c	\
	test_archive_read_multiple_data_objects.c	\
	test_archive_read_next_header_empty.c	\
	test_archive_read_next_header_raw.c	\
	test_archive_read_open2.c		\
	test_archive_read_set_filter_option.c	\
	test_archive_read_set_format_option.c	\
	test_archive_read_set_option.c		\
	test_archive_read_set_options.c		\
	test_archive_read_support.c		\
	test_archive_set_error.c		\
	test_archive_string.c			\
	test_archive_string_conversion.c	\
	test_archive_write_add_filter_by_name.c	\
	test_archive_write_set_filter_option.c	\
	test_archive_write_set_format_by_name.c	\
	test_archive_write_set_format_option.c	\
	test_archive_write_set_option.c		\
	test_archive_write_set_options.c	\
	test_bad_fd.c				\
	test_compat_bzip2.c			\
	test_compat_cpio.c			\
	test_compat_gtar.c			\
	test_compat_gzip.c			\
	test_compat_lzip.c			\
	test_compat_lzma.c			\
	test_compat_lzop.c			\
	test_compat_mac.c			\
	test_compat_pax_libarchive_2x.c		\
	test_compat_solaris_tar_acl.c		\
	test_compat_solaris_pax_sparse.c	\
	test_compat_tar_hardlink.c		\
	test_compat_uudecode.c			\
	test_compat_xz.c			\
	test_compat_zip.c			\
	test_empty_write.c			\
	test_entry.c				\
	test_entry_strmode.c			\
	test_extattr_freebsd.c			\
	test_filter_count.c			\
	test_fuzz.c				\
	test_gnutar_filename_encoding.c		\
	test_link_resolver.c			\
	test_open_fd.c				\
	test_open_failure.c			\
	test_open_file.c			\
	test_open_filename.c			\
	test_pax_filename_encoding.c		\
	test_read_data_large.c			\
	test_read_disk.c			\
	test_read_disk_directory_traversals.c	\
	test_read_disk_entry_from_file.c	\
	test_read_extract.c			\
	test_read_file_nonexistent.c		\
	test_read_filter_grzip.c		\
	test_read_filter_lrzip.c		\
	test_read_filter_lzop.c			\
	test_read_filter_lzop_multiple_parts.c	\
	test_read_filter_program.c		\
	test_read_filter_program_signature.c	\
	test_read_filter_uudecode.c		\
	test_read_format_7zip.c			\
	test_read_format_ar.c			\
	test_read_format_cab.c			\
	test_read_format_cab_filename.c		\
	test_read_format_cpio_afio.c		\
	test_read_format_cpio_bin.c		\
	test_read_format_cpio_bin_Z.c		\
	test_read_format_cpio_bin_be.c		\
	test_read_format_cpio_bin_bz2.c		\
	test_read_format_cpio_bin_gz.c		\
	test_read_format_cpio_bin_lzip.c	\
	test_read_format_cpio_bin_lzma.c	\
	test_read_format_cpio_bin_xz.c		\
	test_read_format_cpio_filename.c	\
	test_read_format_cpio_odc.c		\
	test_read_format_cpio_svr4_gzip.c	\
	test_read_format_cpio_svr4c_Z.c		\
	test_read_format_cpio_svr4_bzip2_rpm.c	\
	test_read_format_cpio_svr4_gzip_rpm.c	\
	test_read_format_empty.c		\
	test_read_format_gtar_filename.c	\
	test_read_format_gtar_gz.c		\
	test_read_format_gtar_lzma.c		\
	test_read_format_gtar_sparse.c		\
	test_read_format_gtar_sparse_skip_entry.c \
	test_read_format_iso_Z.c		\
	test_read_format_iso_multi_extent.c	\
	test_read_format_iso_xorriso.c		\
	test_read_format_isorr_rr_moved.c	\
	test_read_format_isojoliet_bz2.c	\
	test_read_format_isojoliet_long.c	\
	test_read_format_isojoliet_rr.c		\
	test_read_format_isojoliet_versioned.c	\
	test_read_format_isorr_bz2.c		\
	test_read_format_isorr_ce.c		\
	test_read_format_isorr_new_bz2.c	\
	test_read_format_isozisofs_bz2.c	\
	test_read_format_lha.c			\
	test_read_format_lha_filename.c		\
	test_read_format_mtree.c		\
	test_read_format_pax_bz2.c		\
	test_read_format_rar.c			\
	test_read_format_raw.c			\
	test_read_format_tar.c			\
	test_read_format_tar_empty_filename.c	\
	test_read_format_tar_filename.c		\
	test_read_format_tbz.c			\
	test_read_format_tgz.c			\
	test_read_format_tlz.c			\
	test_read_format_txz.c			\
	test_read_format_tz.c			\
	test_read_format_ustar_filename.c	\
	test_read_format_xar.c			\
	test_read_format_zip.c			\
	test_read_format_zip_comment_stored.c	\
	test_read_format_zip_filename.c		\
	test_read_format_zip_mac_metadata.c	\
	test_read_format_zip_sfx.c		\
	test_read_large.c			\
	test_read_pax_truncated.c		\
	test_read_position.c			\
	test_read_set_format.c			\
	test_read_truncated.c			\
	test_read_truncated_filter.c		\
	test_sparse_basic.c			\
	test_tar_filenames.c			\
	test_tar_large.c			\
	test_ustar_filenames.c			\
	test_ustar_filename_encoding.c		\
	test_write_disk.c			\
	test_write_disk_appledouble.c		\
	test_write_disk_failures.c		\
	test_write_disk_hardlink.c		\
	test_write_disk_hfs_compression.c	\
	test_write_disk_lookup.c		\
	test_write_disk_mac_metadata.c		\
	test_write_disk_no_hfs_compression.c	\
	test_write_disk_perms.c			\
	test_write_disk_secure.c		\
	test_write_disk_sparse.c		\
	test_write_disk_symlink.c		\
	test_write_disk_times.c			\
	test_write_filter_b64encode.c		\
	test_write_filter_bzip2.c		\
	test_write_filter_compress.c		\
	test_write_filter_gzip.c		\
	test_write_filter_gzip_timestamp.c	\
	test_write_filter_lrzip.c		\
	test_write_filter_lzip.c		\
	test_write_filter_lzma.c		\
	test_write_filter_lzop.c		\
	test_write_filter_program.c		\
	test_write_filter_uuencode.c		\
	test_write_filter_xz.c			\
	test_write_format_7zip.c		\
	test_write_format_7zip_empty.c		\
	test_write_format_7zip_large.c		\
	test_write_format_ar.c			\
	test_write_format_cpio.c		\
	test_write_format_cpio_empty.c		\
	test_write_format_cpio_newc.c		\
	test_write_format_cpio_odc.c		\
	test_write_format_gnutar.c		\
	test_write_format_iso9660.c		\
	test_write_format_iso9660_boot.c	\
	test_write_format_iso9660_empty.c	\
	test_write_format_iso9660_filename.c	\
	test_write_format_iso9660_zisofs.c	\
	test_write_format_mtree.c		\
	test_write_format_mtree_absolute_path.c	\
	test_write_format_mtree_classic.c	\
	test_write_format_mtree_classic_indent.c	\
	test_write_format_mtree_fflags.c	\
	test_write_format_mtree_no_separator.c	\
	test_write_format_mtree_quoted_filename.c	\
	test_write_format_pax.c			\
	test_write_format_shar_empty.c		\
	test_write_format_tar.c			\
	test_write_format_tar_empty.c		\
	test_write_format_tar_sparse.c		\
	test_write_format_tar_ustar.c		\
	test_write_format_tar_v7tar.c		\
	test_write_format_xar.c			\
	test_write_format_xar_empty.c		\
	test_write_format_zip.c			\
	test_write_format_zip_empty.c		\
	test_write_format_zip_no_compression.c	\
	test_write_zip_set_compression_store.c	\
	test_write_open_memory.c		\
	test_zip_filename_encoding.c

# Build the test program.
SRCS= \
	${TESTS}				\
	list.h					\
	main.c					\
	read_open_memory.c

.PATH:	${LIBARCHIVEDIR}/test_utils
SRCS+=	test_utils.c

# Build libarchive_test and run it.
check test: libarchive_test
	./libarchive_test -r ${LIBARCHIVEDIR}/libarchive/test

# list.h is just a list of all tests, as indicated by DEFINE_TEST macro lines
list.h: ${TESTS} Makefile
	(cd ${LIBARCHIVEDIR}/libarchive/test; cat test_*.c) | \
	grep DEFINE_TEST > ${.OBJDIR}/list.h

CLEANFILES += *.out *.o *.core *~ list.h .dirstamp .depend
CLEANDIRS += .deps .libs

cleantest:
	-chmod -R +w /tmp/libarchive_test.*
	rm -rf /tmp/libarchive_test.*

.include <bsd.prog.mk>
