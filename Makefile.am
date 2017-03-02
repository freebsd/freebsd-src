## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS= foreign subdir-objects
ACLOCAL_AMFLAGS = -I build/autoconf

#
# What to build and install
#
lib_LTLIBRARIES=	libarchive.la
noinst_LTLIBRARIES=	libarchive_fe.la
bin_PROGRAMS=	$(bsdtar_programs) $(bsdcpio_programs) $(bsdcat_programs)
man_MANS= $(libarchive_man_MANS) $(bsdtar_man_MANS) $(bsdcpio_man_MANS) $(bsdcat_man_MANS)
BUILT_SOURCES= libarchive/test/list.h tar/test/list.h cpio/test/list.h cat/test/list.h

#
# What to test: We always test libarchive, test bsdtar and bsdcpio only
# if we built them.
#
check_PROGRAMS= libarchive_test $(bsdtar_test_programs) $(bsdcpio_test_programs) $(bsdcat_test_programs)
TESTS= libarchive_test $(bsdtar_test_programs) $(bsdcpio_test_programs) $(bsdcat_test_programs)
TESTS_ENVIRONMENT= $(libarchive_TESTS_ENVIRONMENT) $(bsdtar_TESTS_ENVIRONMENT) $(bsdcpio_TESTS_ENVIRONMENT) $(bsdcat_TESTS_ENVIRONMENT)
# Always build and test both bsdtar and bsdcpio as part of 'distcheck'
DISTCHECK_CONFIGURE_FLAGS = --enable-bsdtar --enable-bsdcpio
# The next line is commented out by default in shipping libarchive releases.
# It is uncommented by default in trunk.
# DEV_CFLAGS=-Werror -Wextra -Wunused -Wshadow -Wmissing-prototypes -Wcast-qual -g
AM_CFLAGS=$(DEV_CFLAGS)
PLATFORMCPPFLAGS = @PLATFORMCPPFLAGS@
AM_CPPFLAGS=$(PLATFORMCPPFLAGS)

#
# What to include in the distribution
#
EXTRA_DIST= \
	CMakeLists.txt \
	README.md \
	build/autogen.sh \
	build/bump-version.sh \
	build/clean.sh \
	build/cmake \
	build/version \
	contrib \
	doc \
	examples \
	$(libarchive_EXTRA_DIST) \
	$(libarchive_test_EXTRA_DIST) \
	$(bsdtar_EXTRA_DIST) \
	$(bsdtar_test_EXTRA_DIST) \
	$(bsdcpio_EXTRA_DIST) \
	$(bsdcpio_test_EXTRA_DIST) \
	$(bsdcat_EXTRA_DIST) \
	$(bsdcat_test_EXTRA_DIST)

# a) Clean out some unneeded files and directories
# b) Collect all documentation and format it for distribution.
dist-hook:
	rm -rf `find $(distdir) -name CVS -type d`
	rm -rf `find $(distdir) -name .svn -type d`
	rm -f `find $(distdir) -name '*~'`
	rm -f `find $(distdir) -name '*.out'`
	rm -f `find $(distdir) -name '*.core'`
	-rm -f $(distdir)/*/Makefile $(distdir)/*/*/Makefile
	cd $(distdir)/doc && /bin/sh update.sh

#
# Extra rules for cleanup
#
DISTCLEANFILES= \
	libarchive/test/list.h \
	tar/test/list.h \
	cpio/test/list.h \
	cat/test/list.h

distclean-local:
	-rm -rf .ref
	-rm -rf autom4te.cache/
	-rm -f *~
	-[ -f libarchive/Makefile ] && cd libarchive && make clean
	-[ -f libarchive/test/Makefile ] && cd libarchive/test && make clean
	-[ -f tar/Makefile ] && cd tar && make clean
	-[ -f tar/test/Makefile ] && cd tar/test && make clean
	-[ -f cpio/Makefile ] && cd cpio && make clean
	-[ -f cpio/test/Makefile ] && cd cpio/test && make clean
	-[ -f cat/Makefile ] && cd cat && make clean
	-[ -f cpio/test/Makefile ] && cd cat/test && make clean

#
# Libarchive headers, source, etc.
#
#

include_HEADERS= libarchive/archive.h libarchive/archive_entry.h

libarchive_la_SOURCES= \
	libarchive/archive_acl.c \
	libarchive/archive_acl_private.h \
	libarchive/archive_check_magic.c \
	libarchive/archive_cmdline.c \
	libarchive/archive_cmdline_private.h \
	libarchive/archive_crc32.h \
	libarchive/archive_cryptor.c \
	libarchive/archive_cryptor_private.h \
	libarchive/archive_digest.c \
	libarchive/archive_digest_private.h \
	libarchive/archive_endian.h \
	libarchive/archive_entry.c \
	libarchive/archive_entry.h \
	libarchive/archive_entry_copy_stat.c \
	libarchive/archive_entry_link_resolver.c \
	libarchive/archive_entry_locale.h \
	libarchive/archive_entry_private.h \
	libarchive/archive_entry_sparse.c \
	libarchive/archive_entry_stat.c \
	libarchive/archive_entry_strmode.c \
	libarchive/archive_entry_xattr.c \
	libarchive/archive_getdate.c \
	libarchive/archive_getdate.h \
	libarchive/archive_hmac.c \
	libarchive/archive_hmac_private.h \
	libarchive/archive_match.c \
	libarchive/archive_openssl_evp_private.h \
	libarchive/archive_openssl_hmac_private.h \
	libarchive/archive_options.c \
	libarchive/archive_options_private.h \
	libarchive/archive_pack_dev.h \
	libarchive/archive_pack_dev.c \
	libarchive/archive_pathmatch.c \
	libarchive/archive_pathmatch.h \
	libarchive/archive_platform.h \
	libarchive/archive_ppmd_private.h \
	libarchive/archive_ppmd7.c \
	libarchive/archive_ppmd7_private.h \
	libarchive/archive_private.h \
	libarchive/archive_random.c \
	libarchive/archive_random_private.h \
	libarchive/archive_rb.c \
	libarchive/archive_rb.h \
	libarchive/archive_read.c \
	libarchive/archive_read_add_passphrase.c \
	libarchive/archive_read_append_filter.c \
	libarchive/archive_read_data_into_fd.c \
	libarchive/archive_read_disk_entry_from_file.c \
	libarchive/archive_read_disk_posix.c \
	libarchive/archive_read_disk_private.h \
	libarchive/archive_read_disk_set_standard_lookup.c \
	libarchive/archive_read_extract.c \
	libarchive/archive_read_extract2.c \
	libarchive/archive_read_open_fd.c \
	libarchive/archive_read_open_file.c \
	libarchive/archive_read_open_filename.c \
	libarchive/archive_read_open_memory.c \
	libarchive/archive_read_private.h \
	libarchive/archive_read_set_format.c \
	libarchive/archive_read_set_options.c \
	libarchive/archive_read_support_filter_all.c \
	libarchive/archive_read_support_filter_bzip2.c \
	libarchive/archive_read_support_filter_compress.c \
	libarchive/archive_read_support_filter_grzip.c \
	libarchive/archive_read_support_filter_gzip.c \
	libarchive/archive_read_support_filter_lrzip.c \
	libarchive/archive_read_support_filter_lz4.c \
	libarchive/archive_read_support_filter_lzop.c \
	libarchive/archive_read_support_filter_none.c \
	libarchive/archive_read_support_filter_program.c \
	libarchive/archive_read_support_filter_rpm.c \
	libarchive/archive_read_support_filter_uu.c \
	libarchive/archive_read_support_filter_xz.c \
	libarchive/archive_read_support_format_7zip.c \
	libarchive/archive_read_support_format_all.c \
	libarchive/archive_read_support_format_ar.c \
	libarchive/archive_read_support_format_by_code.c \
	libarchive/archive_read_support_format_cab.c \
	libarchive/archive_read_support_format_cpio.c \
	libarchive/archive_read_support_format_empty.c \
	libarchive/archive_read_support_format_iso9660.c \
	libarchive/archive_read_support_format_lha.c \
	libarchive/archive_read_support_format_mtree.c \
	libarchive/archive_read_support_format_rar.c \
	libarchive/archive_read_support_format_raw.c \
	libarchive/archive_read_support_format_tar.c \
	libarchive/archive_read_support_format_warc.c \
	libarchive/archive_read_support_format_xar.c \
	libarchive/archive_read_support_format_zip.c \
	libarchive/archive_string.c \
	libarchive/archive_string.h \
	libarchive/archive_string_composition.h \
	libarchive/archive_string_sprintf.c \
	libarchive/archive_util.c \
	libarchive/archive_virtual.c \
	libarchive/archive_write.c \
	libarchive/archive_write_disk_acl.c \
	libarchive/archive_write_disk_posix.c \
	libarchive/archive_write_disk_private.h \
	libarchive/archive_write_disk_set_standard_lookup.c \
	libarchive/archive_write_open_fd.c \
	libarchive/archive_write_open_file.c \
	libarchive/archive_write_open_filename.c \
	libarchive/archive_write_open_memory.c \
	libarchive/archive_write_private.h \
	libarchive/archive_write_add_filter.c \
	libarchive/archive_write_add_filter_b64encode.c \
	libarchive/archive_write_add_filter_by_name.c \
	libarchive/archive_write_add_filter_bzip2.c \
	libarchive/archive_write_add_filter_compress.c \
	libarchive/archive_write_add_filter_grzip.c \
	libarchive/archive_write_add_filter_gzip.c \
	libarchive/archive_write_add_filter_lrzip.c \
	libarchive/archive_write_add_filter_lz4.c \
	libarchive/archive_write_add_filter_lzop.c \
	libarchive/archive_write_add_filter_none.c \
	libarchive/archive_write_add_filter_program.c \
	libarchive/archive_write_add_filter_uuencode.c \
	libarchive/archive_write_add_filter_xz.c \
	libarchive/archive_write_set_format.c \
	libarchive/archive_write_set_format_7zip.c \
	libarchive/archive_write_set_format_ar.c \
	libarchive/archive_write_set_format_by_name.c \
	libarchive/archive_write_set_format_cpio.c \
	libarchive/archive_write_set_format_cpio_newc.c \
	libarchive/archive_write_set_format_filter_by_ext.c \
	libarchive/archive_write_set_format_iso9660.c \
	libarchive/archive_write_set_format_mtree.c \
	libarchive/archive_write_set_format_pax.c \
	libarchive/archive_write_set_format_raw.c \
	libarchive/archive_write_set_format_shar.c \
	libarchive/archive_write_set_format_ustar.c \
	libarchive/archive_write_set_format_v7tar.c \
	libarchive/archive_write_set_format_gnutar.c \
	libarchive/archive_write_set_format_warc.c \
	libarchive/archive_write_set_format_xar.c \
	libarchive/archive_write_set_format_zip.c \
	libarchive/archive_write_set_options.c \
	libarchive/archive_write_set_passphrase.c \
	libarchive/archive_xxhash.h \
	libarchive/config_freebsd.h \
	libarchive/filter_fork_posix.c \
	libarchive/filter_fork.h \
	libarchive/xxhash.c

if INC_WINDOWS_FILES
libarchive_la_SOURCES+= \
	libarchive/archive_entry_copy_bhfi.c \
	libarchive/archive_read_disk_windows.c \
	libarchive/archive_windows.h \
	libarchive/archive_windows.c \
	libarchive/archive_write_disk_windows.c \
	libarchive/filter_fork_windows.c
endif

# -no-undefined marks that libarchive doesn't rely on symbols
# defined in the application.  This is mandatory for cygwin.
libarchive_la_LDFLAGS= -no-undefined -version-info $(ARCHIVE_LIBTOOL_VERSION)
libarchive_la_LIBADD= $(LTLIBICONV)

# Manpages to install
libarchive_man_MANS= \
	libarchive/archive_entry.3 \
	libarchive/archive_entry_acl.3 \
	libarchive/archive_entry_linkify.3 \
	libarchive/archive_entry_paths.3 \
	libarchive/archive_entry_perms.3 \
	libarchive/archive_entry_stat.3 \
	libarchive/archive_entry_time.3 \
	libarchive/archive_read.3 \
	libarchive/archive_read_add_passphrase.3 \
	libarchive/archive_read_data.3 \
	libarchive/archive_read_disk.3 \
	libarchive/archive_read_extract.3 \
	libarchive/archive_read_filter.3 \
	libarchive/archive_read_format.3 \
	libarchive/archive_read_free.3 \
	libarchive/archive_read_header.3 \
	libarchive/archive_read_new.3 \
	libarchive/archive_read_open.3 \
	libarchive/archive_read_set_options.3 \
	libarchive/archive_util.3 \
	libarchive/archive_write.3 \
	libarchive/archive_write_blocksize.3 \
	libarchive/archive_write_data.3 \
	libarchive/archive_write_disk.3 \
	libarchive/archive_write_filter.3 \
	libarchive/archive_write_finish_entry.3 \
	libarchive/archive_write_format.3 \
	libarchive/archive_write_free.3 \
	libarchive/archive_write_header.3 \
	libarchive/archive_write_new.3 \
	libarchive/archive_write_open.3 \
	libarchive/archive_write_set_options.3 \
	libarchive/archive_write_set_passphrase.3 \
	libarchive/cpio.5 \
	libarchive/libarchive.3 \
	libarchive/libarchive_changes.3 \
	libarchive/libarchive_internals.3 \
	libarchive/libarchive-formats.5 \
	libarchive/mtree.5 \
	libarchive/tar.5

# Additional libarchive files to include in the distribution
libarchive_EXTRA_DIST= \
	libarchive/archive_windows.c \
	libarchive/archive_windows.h \
	libarchive/filter_fork_windows.c \
	libarchive/CMakeLists.txt \
	$(libarchive_man_MANS)

# pkgconfig
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = build/pkgconfig/libarchive.pc

# Sources needed by all test programs
test_utils_SOURCES= \
	test_utils/test_utils.c \
	test_utils/test_utils.h \
	test_utils/test_main.c \
	test_utils/test_common.h

#
#
# libarchive_test program
#
#
libarchive_test_SOURCES= \
	$(libarchive_la_SOURCES) \
	$(test_utils_SOURCES) \
	libarchive/test/read_open_memory.c \
	libarchive/test/test.h \
	libarchive/test/test_acl_nfs4.c \
	libarchive/test/test_acl_pax.c \
	libarchive/test/test_acl_platform_nfs4.c \
	libarchive/test/test_acl_platform_posix1e.c \
	libarchive/test/test_acl_posix1e.c \
	libarchive/test/test_acl_text.c \
	libarchive/test/test_archive_api_feature.c \
	libarchive/test/test_archive_clear_error.c \
	libarchive/test/test_archive_cmdline.c \
	libarchive/test/test_archive_digest.c \
	libarchive/test/test_archive_getdate.c \
	libarchive/test/test_archive_match_owner.c \
	libarchive/test/test_archive_match_path.c \
	libarchive/test/test_archive_match_time.c \
	libarchive/test/test_archive_pathmatch.c \
	libarchive/test/test_archive_read_add_passphrase.c \
	libarchive/test/test_archive_read_close_twice.c \
	libarchive/test/test_archive_read_close_twice_open_fd.c \
	libarchive/test/test_archive_read_close_twice_open_filename.c \
	libarchive/test/test_archive_read_multiple_data_objects.c \
	libarchive/test/test_archive_read_next_header_empty.c \
	libarchive/test/test_archive_read_next_header_raw.c \
	libarchive/test/test_archive_read_open2.c \
	libarchive/test/test_archive_read_set_filter_option.c \
	libarchive/test/test_archive_read_set_format_option.c \
	libarchive/test/test_archive_read_set_option.c \
	libarchive/test/test_archive_read_set_options.c \
	libarchive/test/test_archive_read_support.c \
	libarchive/test/test_archive_set_error.c \
	libarchive/test/test_archive_string.c \
	libarchive/test/test_archive_string_conversion.c \
	libarchive/test/test_archive_write_add_filter_by_name.c \
	libarchive/test/test_archive_write_set_filter_option.c \
	libarchive/test/test_archive_write_set_format_by_name.c \
	libarchive/test/test_archive_write_set_format_filter_by_ext.c \
	libarchive/test/test_archive_write_set_format_option.c \
	libarchive/test/test_archive_write_set_option.c \
	libarchive/test/test_archive_write_set_options.c \
	libarchive/test/test_archive_write_set_passphrase.c \
	libarchive/test/test_bad_fd.c \
	libarchive/test/test_compat_bzip2.c \
	libarchive/test/test_compat_cpio.c \
	libarchive/test/test_compat_gtar.c \
	libarchive/test/test_compat_gzip.c \
	libarchive/test/test_compat_lz4.c \
	libarchive/test/test_compat_lzip.c \
	libarchive/test/test_compat_lzma.c \
	libarchive/test/test_compat_lzop.c \
	libarchive/test/test_compat_mac.c \
	libarchive/test/test_compat_pax_libarchive_2x.c \
	libarchive/test/test_compat_perl_archive_tar.c \
	libarchive/test/test_compat_plexus_archiver_tar.c \
	libarchive/test/test_compat_solaris_tar_acl.c \
	libarchive/test/test_compat_solaris_pax_sparse.c \
	libarchive/test/test_compat_star_acl.c \
	libarchive/test/test_compat_tar_hardlink.c \
	libarchive/test/test_compat_uudecode.c \
	libarchive/test/test_compat_uudecode_large.c \
	libarchive/test/test_compat_xz.c \
	libarchive/test/test_compat_zip.c \
	libarchive/test/test_empty_write.c \
	libarchive/test/test_entry.c \
	libarchive/test/test_entry_strmode.c \
	libarchive/test/test_extattr_freebsd.c \
	libarchive/test/test_filter_count.c \
	libarchive/test/test_fuzz.c \
	libarchive/test/test_gnutar_filename_encoding.c \
	libarchive/test/test_link_resolver.c \
	libarchive/test/test_open_failure.c \
	libarchive/test/test_open_fd.c \
	libarchive/test/test_open_file.c \
	libarchive/test/test_open_filename.c \
	libarchive/test/test_pax_filename_encoding.c \
	libarchive/test/test_read_data_large.c \
	libarchive/test/test_read_disk.c \
	libarchive/test/test_read_disk_directory_traversals.c \
	libarchive/test/test_read_disk_entry_from_file.c \
	libarchive/test/test_read_extract.c \
	libarchive/test/test_read_file_nonexistent.c \
	libarchive/test/test_read_filter_compress.c \
	libarchive/test/test_read_filter_grzip.c \
	libarchive/test/test_read_filter_lrzip.c \
	libarchive/test/test_read_filter_lzop.c \
	libarchive/test/test_read_filter_lzop_multiple_parts.c \
	libarchive/test/test_read_filter_program.c \
	libarchive/test/test_read_filter_program_signature.c \
	libarchive/test/test_read_filter_uudecode.c \
	libarchive/test/test_read_format_7zip.c \
	libarchive/test/test_read_format_7zip_encryption_data.c \
	libarchive/test/test_read_format_7zip_encryption_partially.c \
	libarchive/test/test_read_format_7zip_encryption_header.c \
	libarchive/test/test_read_format_7zip_malformed.c \
	libarchive/test/test_read_format_ar.c \
	libarchive/test/test_read_format_cab.c \
	libarchive/test/test_read_format_cab_filename.c \
	libarchive/test/test_read_format_cpio_afio.c \
	libarchive/test/test_read_format_cpio_bin.c \
	libarchive/test/test_read_format_cpio_bin_Z.c \
	libarchive/test/test_read_format_cpio_bin_be.c \
	libarchive/test/test_read_format_cpio_bin_bz2.c \
	libarchive/test/test_read_format_cpio_bin_gz.c \
	libarchive/test/test_read_format_cpio_bin_le.c \
	libarchive/test/test_read_format_cpio_bin_lzip.c \
	libarchive/test/test_read_format_cpio_bin_lzma.c \
	libarchive/test/test_read_format_cpio_bin_xz.c \
	libarchive/test/test_read_format_cpio_filename.c \
	libarchive/test/test_read_format_cpio_odc.c \
	libarchive/test/test_read_format_cpio_svr4_bzip2_rpm.c \
	libarchive/test/test_read_format_cpio_svr4_gzip.c \
	libarchive/test/test_read_format_cpio_svr4_gzip_rpm.c \
	libarchive/test/test_read_format_cpio_svr4c_Z.c \
	libarchive/test/test_read_format_empty.c \
	libarchive/test/test_read_format_gtar_filename.c \
	libarchive/test/test_read_format_gtar_gz.c \
	libarchive/test/test_read_format_gtar_lzma.c \
	libarchive/test/test_read_format_gtar_sparse.c \
	libarchive/test/test_read_format_gtar_sparse_skip_entry.c \
	libarchive/test/test_read_format_iso_Z.c \
	libarchive/test/test_read_format_iso_multi_extent.c \
	libarchive/test/test_read_format_iso_xorriso.c \
	libarchive/test/test_read_format_isojoliet_bz2.c \
	libarchive/test/test_read_format_isojoliet_long.c \
	libarchive/test/test_read_format_isojoliet_rr.c \
	libarchive/test/test_read_format_isojoliet_versioned.c \
	libarchive/test/test_read_format_isorr_bz2.c \
	libarchive/test/test_read_format_isorr_ce.c \
	libarchive/test/test_read_format_isorr_new_bz2.c \
	libarchive/test/test_read_format_isorr_rr_moved.c \
	libarchive/test/test_read_format_isozisofs_bz2.c \
	libarchive/test/test_read_format_lha.c \
	libarchive/test/test_read_format_lha_bugfix_0.c \
	libarchive/test/test_read_format_lha_filename.c \
	libarchive/test/test_read_format_mtree.c \
	libarchive/test/test_read_format_mtree_crash747.c \
	libarchive/test/test_read_format_pax_bz2.c \
	libarchive/test/test_read_format_rar.c \
	libarchive/test/test_read_format_rar_encryption_data.c \
	libarchive/test/test_read_format_rar_encryption_partially.c \
	libarchive/test/test_read_format_rar_encryption_header.c \
	libarchive/test/test_read_format_rar_invalid1.c \
	libarchive/test/test_read_format_raw.c \
	libarchive/test/test_read_format_tar.c \
	libarchive/test/test_read_format_tar_concatenated.c \
	libarchive/test/test_read_format_tar_empty_pax.c \
	libarchive/test/test_read_format_tar_empty_filename.c \
	libarchive/test/test_read_format_tar_filename.c \
	libarchive/test/test_read_format_tbz.c \
	libarchive/test/test_read_format_tgz.c \
	libarchive/test/test_read_format_tlz.c \
	libarchive/test/test_read_format_txz.c \
	libarchive/test/test_read_format_tz.c \
	libarchive/test/test_read_format_ustar_filename.c \
	libarchive/test/test_read_format_warc.c \
	libarchive/test/test_read_format_xar.c \
	libarchive/test/test_read_format_zip.c \
	libarchive/test/test_read_format_zip_comment_stored.c \
	libarchive/test/test_read_format_zip_encryption_data.c \
	libarchive/test/test_read_format_zip_encryption_partially.c \
	libarchive/test/test_read_format_zip_encryption_header.c \
	libarchive/test/test_read_format_zip_filename.c \
	libarchive/test/test_read_format_zip_high_compression.c \
	libarchive/test/test_read_format_zip_jar.c \
	libarchive/test/test_read_format_zip_mac_metadata.c \
	libarchive/test/test_read_format_zip_malformed.c \
	libarchive/test/test_read_format_zip_msdos.c \
	libarchive/test/test_read_format_zip_nested.c \
	libarchive/test/test_read_format_zip_nofiletype.c \
	libarchive/test/test_read_format_zip_padded.c \
	libarchive/test/test_read_format_zip_sfx.c \
	libarchive/test/test_read_format_zip_traditional_encryption_data.c \
	libarchive/test/test_read_format_zip_winzip_aes.c \
	libarchive/test/test_read_format_zip_winzip_aes_large.c \
	libarchive/test/test_read_format_zip_zip64.c \
	libarchive/test/test_read_large.c \
	libarchive/test/test_read_pax_schily_xattr.c \
	libarchive/test/test_read_pax_truncated.c \
	libarchive/test/test_read_position.c \
	libarchive/test/test_read_set_format.c \
	libarchive/test/test_read_too_many_filters.c \
	libarchive/test/test_read_truncated.c \
	libarchive/test/test_read_truncated_filter.c \
	libarchive/test/test_sparse_basic.c \
	libarchive/test/test_tar_filenames.c \
	libarchive/test/test_tar_large.c \
	libarchive/test/test_ustar_filenames.c \
	libarchive/test/test_ustar_filename_encoding.c \
	libarchive/test/test_warn_missing_hardlink_target.c \
	libarchive/test/test_write_disk.c \
	libarchive/test/test_write_disk_appledouble.c \
	libarchive/test/test_write_disk_failures.c \
	libarchive/test/test_write_disk_hardlink.c \
	libarchive/test/test_write_disk_hfs_compression.c \
	libarchive/test/test_write_disk_lookup.c \
	libarchive/test/test_write_disk_mac_metadata.c \
	libarchive/test/test_write_disk_no_hfs_compression.c \
	libarchive/test/test_write_disk_perms.c \
	libarchive/test/test_write_disk_secure.c \
	libarchive/test/test_write_disk_secure744.c \
	libarchive/test/test_write_disk_secure745.c \
	libarchive/test/test_write_disk_secure746.c \
	libarchive/test/test_write_disk_sparse.c \
	libarchive/test/test_write_disk_symlink.c \
	libarchive/test/test_write_disk_times.c \
	libarchive/test/test_write_filter_b64encode.c \
	libarchive/test/test_write_filter_bzip2.c \
	libarchive/test/test_write_filter_compress.c \
	libarchive/test/test_write_filter_gzip.c \
	libarchive/test/test_write_filter_gzip_timestamp.c \
	libarchive/test/test_write_filter_lrzip.c \
	libarchive/test/test_write_filter_lz4.c \
	libarchive/test/test_write_filter_lzip.c \
	libarchive/test/test_write_filter_lzma.c \
	libarchive/test/test_write_filter_lzop.c \
	libarchive/test/test_write_filter_program.c \
	libarchive/test/test_write_filter_uuencode.c \
	libarchive/test/test_write_filter_xz.c \
	libarchive/test/test_write_format_7zip.c \
	libarchive/test/test_write_format_7zip_empty.c \
	libarchive/test/test_write_format_7zip_large.c \
	libarchive/test/test_write_format_ar.c \
	libarchive/test/test_write_format_cpio.c \
	libarchive/test/test_write_format_cpio_empty.c \
	libarchive/test/test_write_format_cpio_newc.c \
	libarchive/test/test_write_format_cpio_odc.c \
	libarchive/test/test_write_format_gnutar.c \
	libarchive/test/test_write_format_gnutar_filenames.c \
	libarchive/test/test_write_format_iso9660.c \
	libarchive/test/test_write_format_iso9660_boot.c \
	libarchive/test/test_write_format_iso9660_empty.c \
	libarchive/test/test_write_format_iso9660_filename.c \
	libarchive/test/test_write_format_iso9660_zisofs.c \
	libarchive/test/test_write_format_mtree.c \
	libarchive/test/test_write_format_mtree_absolute_path.c \
	libarchive/test/test_write_format_mtree_classic.c \
	libarchive/test/test_write_format_mtree_classic_indent.c\
	libarchive/test/test_write_format_mtree_fflags.c \
	libarchive/test/test_write_format_mtree_no_separator.c \
	libarchive/test/test_write_format_mtree_quoted_filename.c\
	libarchive/test/test_write_format_pax.c \
	libarchive/test/test_write_format_raw.c \
	libarchive/test/test_write_format_raw_b64.c \
	libarchive/test/test_write_format_shar_empty.c \
	libarchive/test/test_write_format_tar.c \
	libarchive/test/test_write_format_tar_empty.c \
	libarchive/test/test_write_format_tar_sparse.c \
	libarchive/test/test_write_format_tar_ustar.c \
	libarchive/test/test_write_format_tar_v7tar.c \
	libarchive/test/test_write_format_warc.c \
	libarchive/test/test_write_format_warc_empty.c \
	libarchive/test/test_write_format_xar.c \
	libarchive/test/test_write_format_xar_empty.c \
	libarchive/test/test_write_format_zip.c \
	libarchive/test/test_write_format_zip_compression_store.c \
	libarchive/test/test_write_format_zip_empty.c \
	libarchive/test/test_write_format_zip_empty_zip64.c \
	libarchive/test/test_write_format_zip_file.c \
	libarchive/test/test_write_format_zip_file_zip64.c \
	libarchive/test/test_write_format_zip_large.c \
	libarchive/test/test_write_format_zip_zip64.c \
	libarchive/test/test_write_open_memory.c \
	libarchive/test/test_write_read_format_zip.c \
	libarchive/test/test_zip_filename_encoding.c

libarchive_test_CPPFLAGS= \
	-I$(top_srcdir)/libarchive \
	-I$(top_srcdir)/libarchive/test \
	-I$(top_srcdir)/test_utils \
	-I$(top_builddir)/libarchive/test \
	-DLIBARCHIVE_STATIC $(PLATFORMCPPFLAGS)

libarchive_test_LDADD= $(LTLIBICONV)

# The "list.h" file just lists all of the tests defined in all of the sources.
# Building it automatically provides a sanity-check on libarchive_test_SOURCES
# above.
libarchive/test/list.h: Makefile
	$(MKDIR_P) libarchive/test
	cat $(top_srcdir)/libarchive/test/test_*.c | grep '^DEFINE_TEST' > libarchive/test/list.h

libarchive_TESTS_ENVIRONMENT= LIBARCHIVE_TEST_FILES=`cd $(top_srcdir);/bin/pwd`/libarchive/test LRZIP=NOCONFIG

libarchive_test_EXTRA_DIST=\
	libarchive/test/list.h \
	libarchive/test/test_acl_pax_posix1e.tar.uu \
	libarchive/test/test_acl_pax_nfs4.tar.uu \
	libarchive/test/test_archive_string_conversion.txt.Z.uu \
	libarchive/test/test_compat_bzip2_1.tbz.uu \
	libarchive/test/test_compat_bzip2_2.tbz.uu \
	libarchive/test/test_compat_cpio_1.cpio.uu \
	libarchive/test/test_compat_gtar_1.tar.uu \
	libarchive/test/test_compat_gtar_2.tar.uu \
	libarchive/test/test_compat_gzip_1.tgz.uu \
	libarchive/test/test_compat_gzip_2.tgz.uu \
	libarchive/test/test_compat_lz4_1.tar.lz4.uu \
	libarchive/test/test_compat_lz4_2.tar.lz4.uu \
	libarchive/test/test_compat_lz4_3.tar.lz4.uu \
	libarchive/test/test_compat_lz4_B4.tar.lz4.uu \
	libarchive/test/test_compat_lz4_B4BD.tar.lz4.uu \
	libarchive/test/test_compat_lz4_B4BDBX.tar.lz4.uu \
	libarchive/test/test_compat_lz4_B5.tar.lz4.uu \
	libarchive/test/test_compat_lz4_B5BD.tar.lz4.uu \
	libarchive/test/test_compat_lz4_B6.tar.lz4.uu \
	libarchive/test/test_compat_lz4_B6BD.tar.lz4.uu \
	libarchive/test/test_compat_lz4_B7.tar.lz4.uu \
	libarchive/test/test_compat_lz4_B7BD.tar.lz4.uu \
	libarchive/test/test_compat_lzip_1.tlz.uu \
	libarchive/test/test_compat_lzip_2.tlz.uu \
	libarchive/test/test_compat_lzma_1.tlz.uu \
	libarchive/test/test_compat_lzma_2.tlz.uu \
	libarchive/test/test_compat_lzma_3.tlz.uu \
	libarchive/test/test_compat_lzop_1.tar.lzo.uu \
	libarchive/test/test_compat_lzop_2.tar.lzo.uu \
	libarchive/test/test_compat_lzop_3.tar.lzo.uu \
	libarchive/test/test_compat_mac-1.tar.Z.uu \
	libarchive/test/test_compat_mac-2.tar.Z.uu \
	libarchive/test/test_compat_pax_libarchive_2x.tar.Z.uu \
	libarchive/test/test_compat_perl_archive_tar.tar.uu \
	libarchive/test/test_compat_plexus_archiver_tar.tar.uu \
	libarchive/test/test_compat_solaris_pax_sparse_1.pax.Z.uu \
	libarchive/test/test_compat_solaris_pax_sparse_2.pax.Z.uu \
	libarchive/test/test_compat_solaris_tar_acl.tar.uu \
	libarchive/test/test_compat_star_acl_nfs4.tar.uu \
	libarchive/test/test_compat_star_acl_posix1e.tar.uu \
	libarchive/test/test_compat_tar_hardlink_1.tar.uu \
	libarchive/test/test_compat_uudecode_large.tar.Z.uu \
	libarchive/test/test_compat_xz_1.txz.uu \
	libarchive/test/test_compat_zip_1.zip.uu \
	libarchive/test/test_compat_zip_2.zip.uu \
	libarchive/test/test_compat_zip_3.zip.uu \
	libarchive/test/test_compat_zip_4.zip.uu \
	libarchive/test/test_compat_zip_5.zip.uu \
	libarchive/test/test_compat_zip_6.zip.uu \
	libarchive/test/test_compat_zip_7.xps.uu \
	libarchive/test/test_fuzz.cab.uu \
	libarchive/test/test_fuzz.lzh.uu \
	libarchive/test/test_fuzz_1.iso.Z.uu \
	libarchive/test/test_pax_filename_encoding.tar.uu \
	libarchive/test/test_rar_multivolume_multiple_files.part1.rar.uu \
	libarchive/test/test_rar_multivolume_multiple_files.part2.rar.uu \
	libarchive/test/test_rar_multivolume_multiple_files.part3.rar.uu \
	libarchive/test/test_rar_multivolume_multiple_files.part4.rar.uu \
	libarchive/test/test_rar_multivolume_multiple_files.part5.rar.uu \
	libarchive/test/test_rar_multivolume_multiple_files.part6.rar.uu \
	libarchive/test/test_rar_multivolume_single_file.part1.rar.uu \
	libarchive/test/test_rar_multivolume_single_file.part2.rar.uu \
	libarchive/test/test_rar_multivolume_single_file.part3.rar.uu \
	libarchive/test/test_rar_multivolume_uncompressed_files.part01.rar.uu \
	libarchive/test/test_rar_multivolume_uncompressed_files.part02.rar.uu \
	libarchive/test/test_rar_multivolume_uncompressed_files.part03.rar.uu \
	libarchive/test/test_rar_multivolume_uncompressed_files.part04.rar.uu \
	libarchive/test/test_rar_multivolume_uncompressed_files.part05.rar.uu \
	libarchive/test/test_rar_multivolume_uncompressed_files.part06.rar.uu \
	libarchive/test/test_rar_multivolume_uncompressed_files.part07.rar.uu \
	libarchive/test/test_rar_multivolume_uncompressed_files.part08.rar.uu \
	libarchive/test/test_rar_multivolume_uncompressed_files.part09.rar.uu \
	libarchive/test/test_rar_multivolume_uncompressed_files.part10.rar.uu \
	libarchive/test/test_read_filter_grzip.tar.grz.uu \
	libarchive/test/test_read_filter_lrzip.tar.lrz.uu \
	libarchive/test/test_read_filter_lzop.tar.lzo.uu \
	libarchive/test/test_read_filter_lzop_multiple_parts.tar.lzo.uu \
	libarchive/test/test_read_format_mtree_crash747.mtree.bz2.uu \
	libarchive/test/test_read_format_7zip_bcj2_bzip2.7z.uu \
	libarchive/test/test_read_format_7zip_bcj2_copy_1.7z.uu \
	libarchive/test/test_read_format_7zip_bcj2_copy_2.7z.uu \
	libarchive/test/test_read_format_7zip_bcj2_copy_lzma.7z.uu \
	libarchive/test/test_read_format_7zip_bcj2_deflate.7z.uu \
	libarchive/test/test_read_format_7zip_bcj2_lzma1_1.7z.uu \
	libarchive/test/test_read_format_7zip_bcj2_lzma1_2.7z.uu \
	libarchive/test/test_read_format_7zip_bcj2_lzma2_1.7z.uu \
	libarchive/test/test_read_format_7zip_bcj2_lzma2_2.7z.uu \
	libarchive/test/test_read_format_7zip_bcj_bzip2.7z.uu \
	libarchive/test/test_read_format_7zip_bcj_copy.7z.uu \
	libarchive/test/test_read_format_7zip_bcj_deflate.7z.uu \
	libarchive/test/test_read_format_7zip_bcj_lzma1.7z.uu \
	libarchive/test/test_read_format_7zip_bcj_lzma2.7z.uu \
	libarchive/test/test_read_format_7zip_bzip2.7z.uu \
	libarchive/test/test_read_format_7zip_copy.7z.uu \
	libarchive/test/test_read_format_7zip_copy_2.7z.uu \
	libarchive/test/test_read_format_7zip_deflate.7z.uu \
	libarchive/test/test_read_format_7zip_delta_lzma1.7z.uu \
	libarchive/test/test_read_format_7zip_delta_lzma2.7z.uu \
	libarchive/test/test_read_format_7zip_empty_archive.7z.uu \
	libarchive/test/test_read_format_7zip_empty_file.7z.uu \
	libarchive/test/test_read_format_7zip_encryption.7z.uu \
	libarchive/test/test_read_format_7zip_encryption_header.7z.uu \
	libarchive/test/test_read_format_7zip_encryption_partially.7z.uu \
	libarchive/test/test_read_format_7zip_lzma1.7z.uu \
	libarchive/test/test_read_format_7zip_lzma1_2.7z.uu \
	libarchive/test/test_read_format_7zip_lzma1_lzma2.7z.uu \
	libarchive/test/test_read_format_7zip_lzma2.7z.uu \
	libarchive/test/test_read_format_7zip_malformed.7z.uu \
	libarchive/test/test_read_format_7zip_malformed2.7z.uu \
	libarchive/test/test_read_format_7zip_ppmd.7z.uu \
	libarchive/test/test_read_format_7zip_symbolic_name.7z.uu \
	libarchive/test/test_read_format_ar.ar.uu \
	libarchive/test/test_read_format_cab_1.cab.uu \
	libarchive/test/test_read_format_cab_2.cab.uu \
	libarchive/test/test_read_format_cab_3.cab.uu \
	libarchive/test/test_read_format_cab_filename_cp932.cab.uu \
	libarchive/test/test_read_format_cpio_bin_be.cpio.uu \
	libarchive/test/test_read_format_cpio_bin_le.cpio.uu \
	libarchive/test/test_read_format_cpio_filename_cp866.cpio.uu \
	libarchive/test/test_read_format_cpio_filename_eucjp.cpio.uu \
	libarchive/test/test_read_format_cpio_filename_koi8r.cpio.uu \
	libarchive/test/test_read_format_cpio_filename_utf8_jp.cpio.uu \
	libarchive/test/test_read_format_cpio_filename_utf8_ru.cpio.uu \
	libarchive/test/test_read_format_cpio_svr4_bzip2_rpm.rpm.uu \
	libarchive/test/test_read_format_cpio_svr4_gzip_rpm.rpm.uu \
	libarchive/test/test_read_format_gtar_filename_cp866.tar.Z.uu \
	libarchive/test/test_read_format_gtar_filename_eucjp.tar.Z.uu \
	libarchive/test/test_read_format_gtar_filename_koi8r.tar.Z.uu \
	libarchive/test/test_read_format_gtar_sparse_1_13.tar.uu \
	libarchive/test/test_read_format_gtar_sparse_1_17.tar.uu \
	libarchive/test/test_read_format_gtar_sparse_1_17_posix00.tar.uu \
	libarchive/test/test_read_format_gtar_sparse_1_17_posix01.tar.uu \
	libarchive/test/test_read_format_gtar_sparse_1_17_posix10.tar.uu \
	libarchive/test/test_read_format_gtar_sparse_1_17_posix10_modified.tar.uu \
	libarchive/test/test_read_format_gtar_sparse_skip_entry.tar.Z.uu \
	libarchive/test/test_read_format_iso.iso.Z.uu \
	libarchive/test/test_read_format_iso_2.iso.Z.uu \
	libarchive/test/test_read_format_iso_joliet.iso.Z.uu \
	libarchive/test/test_read_format_iso_joliet_by_nero.iso.Z.uu \
	libarchive/test/test_read_format_iso_joliet_long.iso.Z.uu \
	libarchive/test/test_read_format_iso_joliet_rockridge.iso.Z.uu \
	libarchive/test/test_read_format_iso_multi_extent.iso.Z.uu \
	libarchive/test/test_read_format_iso_rockridge.iso.Z.uu \
	libarchive/test/test_read_format_iso_rockridge_ce.iso.Z.uu \
	libarchive/test/test_read_format_iso_rockridge_new.iso.Z.uu \
	libarchive/test/test_read_format_iso_rockridge_rr_moved.iso.Z.uu \
	libarchive/test/test_read_format_iso_xorriso.iso.Z.uu \
	libarchive/test/test_read_format_iso_zisofs.iso.Z.uu \
	libarchive/test/test_read_format_lha_bugfix_0.lzh.uu \
	libarchive/test/test_read_format_lha_filename_cp932.lzh.uu \
	libarchive/test/test_read_format_lha_header0.lzh.uu \
	libarchive/test/test_read_format_lha_header1.lzh.uu \
	libarchive/test/test_read_format_lha_header2.lzh.uu \
	libarchive/test/test_read_format_lha_header3.lzh.uu \
	libarchive/test/test_read_format_lha_lh0.lzh.uu \
	libarchive/test/test_read_format_lha_lh6.lzh.uu \
	libarchive/test/test_read_format_lha_lh7.lzh.uu \
	libarchive/test/test_read_format_lha_withjunk.lzh.uu \
	libarchive/test/test_read_format_mtree.mtree.uu \
	libarchive/test/test_read_format_mtree_nomagic.mtree.uu \
	libarchive/test/test_read_format_mtree_nomagic2.mtree.uu \
	libarchive/test/test_read_format_mtree_nomagic3.mtree.uu \
	libarchive/test/test_read_format_rar.rar.uu \
	libarchive/test/test_read_format_rar_binary_data.rar.uu \
	libarchive/test/test_read_format_rar_compress_best.rar.uu \
	libarchive/test/test_read_format_rar_compress_normal.rar.uu \
	libarchive/test/test_read_format_rar_encryption_data.rar.uu \
	libarchive/test/test_read_format_rar_encryption_header.rar.uu \
	libarchive/test/test_read_format_rar_encryption_partially.rar.uu \
	libarchive/test/test_read_format_rar_invalid1.rar.uu \
	libarchive/test/test_read_format_rar_multi_lzss_blocks.rar.uu \
	libarchive/test/test_read_format_rar_multivolume.part0001.rar.uu \
	libarchive/test/test_read_format_rar_multivolume.part0002.rar.uu \
	libarchive/test/test_read_format_rar_multivolume.part0003.rar.uu \
	libarchive/test/test_read_format_rar_multivolume.part0004.rar.uu \
	libarchive/test/test_read_format_rar_noeof.rar.uu \
	libarchive/test/test_read_format_rar_ppmd_lzss_conversion.rar.uu \
	libarchive/test/test_read_format_rar_sfx.exe.uu \
	libarchive/test/test_read_format_rar_subblock.rar.uu \
	libarchive/test/test_read_format_rar_unicode.rar.uu \
	libarchive/test/test_read_format_rar_windows.rar.uu \
	libarchive/test/test_read_format_raw.bufr.uu \
	libarchive/test/test_read_format_raw.data.Z.uu \
	libarchive/test/test_read_format_raw.data.uu \
	libarchive/test/test_read_format_tar_concatenated.tar.uu \
	libarchive/test/test_read_format_tar_empty_filename.tar.uu \
	libarchive/test/test_read_format_tar_empty_pax.tar.Z.uu \
	libarchive/test/test_read_format_tar_filename_koi8r.tar.Z.uu \
	libarchive/test/test_read_format_ustar_filename_cp866.tar.Z.uu \
	libarchive/test/test_read_format_ustar_filename_eucjp.tar.Z.uu \
	libarchive/test/test_read_format_ustar_filename_koi8r.tar.Z.uu \
	libarchive/test/test_read_format_warc.warc.uu \
	libarchive/test/test_read_format_zip.zip.uu \
	libarchive/test/test_read_format_zip_comment_stored_1.zip.uu \
	libarchive/test/test_read_format_zip_comment_stored_2.zip.uu \
	libarchive/test/test_read_format_zip_encryption_data.zip.uu \
	libarchive/test/test_read_format_zip_encryption_header.zip.uu \
	libarchive/test/test_read_format_zip_encryption_partially.zip.uu \
	libarchive/test/test_read_format_zip_filename_cp866.zip.uu \
	libarchive/test/test_read_format_zip_filename_cp932.zip.uu \
	libarchive/test/test_read_format_zip_filename_koi8r.zip.uu \
	libarchive/test/test_read_format_zip_filename_utf8_jp.zip.uu \
	libarchive/test/test_read_format_zip_filename_utf8_ru.zip.uu \
	libarchive/test/test_read_format_zip_filename_utf8_ru2.zip.uu \
	libarchive/test/test_read_format_zip_high_compression.zip.uu \
	libarchive/test/test_read_format_zip_length_at_end.zip.uu \
	libarchive/test/test_read_format_zip_jar.jar.uu \
	libarchive/test/test_read_format_zip_mac_metadata.zip.uu \
	libarchive/test/test_read_format_zip_malformed1.zip.uu \
	libarchive/test/test_read_format_zip_msdos.zip.uu \
	libarchive/test/test_read_format_zip_nested.zip.uu \
	libarchive/test/test_read_format_zip_nofiletype.zip.uu \
	libarchive/test/test_read_format_zip_padded1.zip.uu \
	libarchive/test/test_read_format_zip_padded2.zip.uu \
	libarchive/test/test_read_format_zip_padded3.zip.uu \
	libarchive/test/test_read_format_zip_sfx.uu \
	libarchive/test/test_read_format_zip_symlink.zip.uu \
	libarchive/test/test_read_format_zip_traditional_encryption_data.zip.uu \
	libarchive/test/test_read_format_zip_ux.zip.uu \
	libarchive/test/test_read_format_zip_winzip_aes128.zip.uu \
	libarchive/test/test_read_format_zip_winzip_aes256.zip.uu \
	libarchive/test/test_read_format_zip_winzip_aes256_large.zip.uu \
	libarchive/test/test_read_format_zip_winzip_aes256_stored.zip.uu \
	libarchive/test/test_read_format_zip_zip64a.zip.uu \
	libarchive/test/test_read_format_zip_zip64b.zip.uu \
	libarchive/test/test_read_large_splitted_rar_aa.uu \
	libarchive/test/test_read_large_splitted_rar_ab.uu \
	libarchive/test/test_read_large_splitted_rar_ac.uu \
	libarchive/test/test_read_large_splitted_rar_ad.uu \
	libarchive/test/test_read_large_splitted_rar_ae.uu \
	libarchive/test/test_read_pax_schily_xattr.tar.uu \
	libarchive/test/test_read_splitted_rar_aa.uu \
	libarchive/test/test_read_splitted_rar_ab.uu \
	libarchive/test/test_read_splitted_rar_ac.uu \
	libarchive/test/test_read_splitted_rar_ad.uu \
	libarchive/test/test_read_too_many_filters.gz.uu \
	libarchive/test/test_splitted_rar_seek_support_aa.uu \
	libarchive/test/test_splitted_rar_seek_support_ab.uu \
	libarchive/test/test_splitted_rar_seek_support_ac.uu \
	libarchive/test/test_write_disk_appledouble.cpio.gz.uu \
	libarchive/test/test_write_disk_hfs_compression.tgz.uu \
	libarchive/test/test_write_disk_mac_metadata.tar.gz.uu \
	libarchive/test/test_write_disk_no_hfs_compression.tgz.uu \
	libarchive/test/CMakeLists.txt \
	libarchive/test/README

#
# Common code for libarchive frontends (cpio, tar)
#
libarchive_fe_la_SOURCES= \
	libarchive_fe/err.c \
	libarchive_fe/err.h \
	libarchive_fe/lafe_platform.h \
	libarchive_fe/line_reader.c \
	libarchive_fe/line_reader.h \
	libarchive_fe/passphrase.c \
	libarchive_fe/passphrase.h

libarchive_fe_la_CPPFLAGS= -I$(top_srcdir)/libarchive
#
#
# bsdtar source, docs, etc.
#
#

bsdtar_SOURCES= \
		tar/bsdtar.c \
		tar/bsdtar.h \
		tar/bsdtar_platform.h \
		tar/cmdline.c \
		tar/creation_set.c \
		tar/read.c \
		tar/subst.c \
		tar/util.c \
		tar/write.c

if INC_WINDOWS_FILES
bsdtar_SOURCES+= \
		tar/bsdtar_windows.h \
		tar/bsdtar_windows.c
endif

bsdtar_DEPENDENCIES= libarchive.la libarchive_fe.la

if STATIC_BSDTAR
bsdtar_ldstatic= -static
bsdtar_ccstatic= -DLIBARCHIVE_STATIC
else
bsdtar_ldstatic=
bsdtar_ccstatic=
endif

bsdtar_LDADD= libarchive.la libarchive_fe.la $(LTLIBICONV)
bsdtar_CPPFLAGS= -I$(top_srcdir)/libarchive -I$(top_srcdir)/libarchive_fe $(bsdtar_ccstatic) $(PLATFORMCPPFLAGS)
bsdtar_LDFLAGS= $(bsdtar_ldstatic)

bsdtar_EXTRA_DIST= \
	tar/bsdtar.1 \
	tar/bsdtar_windows.h \
	tar/bsdtar_windows.c \
	tar/CMakeLists.txt \
	tar/config_freebsd.h


if BUILD_BSDTAR
bsdtar_man_MANS= tar/bsdtar.1
bsdtar_programs= bsdtar
else
bsdtar_man_MANS=
bsdtar_programs=
endif

#
# bsdtar_test
#

bsdtar_test_SOURCES= \
	$(test_utils_SOURCES) \
	tar/test/test.h \
	tar/test/test_0.c \
	tar/test/test_basic.c \
	tar/test/test_copy.c \
	tar/test/test_empty_mtree.c \
	tar/test/test_extract_tar_Z.c \
	tar/test/test_extract_tar_bz2.c \
	tar/test/test_extract_tar_grz.c \
	tar/test/test_extract_tar_gz.c \
	tar/test/test_extract_tar_lrz.c \
	tar/test/test_extract_tar_lz.c \
	tar/test/test_extract_tar_lz4.c \
	tar/test/test_extract_tar_lzma.c \
	tar/test/test_extract_tar_lzo.c \
	tar/test/test_extract_tar_xz.c \
	tar/test/test_format_newc.c \
	tar/test/test_help.c \
	tar/test/test_leading_slash.c \
	tar/test/test_missing_file.c \
	tar/test/test_option_C_upper.c \
	tar/test/test_option_H_upper.c \
	tar/test/test_option_L_upper.c \
	tar/test/test_option_O_upper.c \
	tar/test/test_option_T_upper.c \
	tar/test/test_option_U_upper.c \
	tar/test/test_option_X_upper.c \
	tar/test/test_option_a.c \
	tar/test/test_option_b.c \
	tar/test/test_option_b64encode.c \
	tar/test/test_option_exclude.c \
	tar/test/test_option_gid_gname.c \
	tar/test/test_option_grzip.c \
	tar/test/test_option_j.c \
	tar/test/test_option_k.c \
	tar/test/test_option_keep_newer_files.c \
	tar/test/test_option_lrzip.c \
	tar/test/test_option_lz4.c \
	tar/test/test_option_lzma.c \
	tar/test/test_option_lzop.c \
	tar/test/test_option_n.c \
	tar/test/test_option_newer_than.c \
	tar/test/test_option_nodump.c \
	tar/test/test_option_older_than.c \
	tar/test/test_option_passphrase.c \
	tar/test/test_option_q.c \
	tar/test/test_option_r.c \
	tar/test/test_option_s.c \
	tar/test/test_option_uid_uname.c \
	tar/test/test_option_uuencode.c \
	tar/test/test_option_xz.c \
	tar/test/test_option_z.c \
	tar/test/test_patterns.c \
	tar/test/test_print_longpath.c \
	tar/test/test_stdio.c \
	tar/test/test_strip_components.c \
	tar/test/test_symlink_dir.c \
	tar/test/test_version.c \
	tar/test/test_windows.c

bsdtar_test_CPPFLAGS=\
	-I$(top_srcdir)/libarchive -I$(top_srcdir)/libarchive_fe \
	-I$(top_srcdir)/test_utils \
	-I$(top_srcdir)/tar -I$(top_srcdir)/tar/test \
	-I$(top_builddir)/tar/test \
	$(PLATFORMCPPFLAGS)

tar/test/list.h: Makefile
	$(MKDIR_P) tar/test
	cat $(top_srcdir)/tar/test/test_*.c | grep '^DEFINE_TEST' > tar/test/list.h

if BUILD_BSDTAR
bsdtar_test_programs= bsdtar_test
bsdtar_TESTS_ENVIRONMENT= BSDTAR=`cd $(top_builddir);/bin/pwd`/bsdtar$(EXEEXT) BSDTAR_TEST_FILES=`cd $(top_srcdir);/bin/pwd`/tar/test
else
bsdtar_test_programs=
bsdtar_TESTS_ENVIRONMENT=
endif

bsdtar_test_EXTRA_DIST= \
	tar/test/list.h \
	tar/test/test_extract.tar.Z.uu \
	tar/test/test_extract.tar.bz2.uu \
	tar/test/test_extract.tar.grz.uu \
	tar/test/test_extract.tar.gz.uu \
	tar/test/test_extract.tar.lrz.uu \
	tar/test/test_extract.tar.lz.uu \
	tar/test/test_extract.tar.lz4.uu \
	tar/test/test_extract.tar.lzma.uu \
	tar/test/test_extract.tar.lzo.uu \
	tar/test/test_extract.tar.xz.uu \
	tar/test/test_leading_slash.tar.uu \
	tar/test/test_option_keep_newer_files.tar.Z.uu \
	tar/test/test_option_passphrase.zip.uu \
	tar/test/test_option_s.tar.Z.uu \
	tar/test/test_patterns_2.tar.uu \
	tar/test/test_patterns_3.tar.uu \
	tar/test/test_patterns_4.tar.uu \
	tar/test/test_print_longpath.tar.Z.uu \
	tar/test/CMakeLists.txt


#
#
# bsdcpio source, docs, etc.
#
#

bsdcpio_SOURCES= \
		cpio/cmdline.c \
		cpio/cpio.c \
		cpio/cpio.h \
		cpio/cpio_platform.h

if INC_WINDOWS_FILES
bsdcpio_SOURCES+= \
		cpio/cpio_windows.h \
		cpio/cpio_windows.c
endif

bsdcpio_DEPENDENCIES = libarchive.la libarchive_fe.la


if STATIC_BSDCPIO
bsdcpio_ldstatic= -static
bsdcpio_ccstatic= -DLIBARCHIVE_STATIC
else
bsdcpio_ldstatic=
bsdcpio_ccstatic=
endif

bsdcpio_LDADD= libarchive_fe.la libarchive.la $(LTLIBICONV)
bsdcpio_CPPFLAGS= -I$(top_srcdir)/libarchive -I$(top_srcdir)/libarchive_fe $(bsdcpio_ccstatic) $(PLATFORMCPPFLAGS)
bsdcpio_LDFLAGS= $(bsdcpio_ldstatic)

bsdcpio_EXTRA_DIST= \
	cpio/bsdcpio.1 \
	cpio/cpio_windows.h \
	cpio/cpio_windows.c \
	cpio/CMakeLists.txt \
	cpio/config_freebsd.h


if BUILD_BSDCPIO
# Manpages to install
bsdcpio_man_MANS= cpio/bsdcpio.1
bsdcpio_programs= bsdcpio
else
bsdcpio_man_MANS=
bsdcpio_programs=
endif

#
# bsdcpio_test
#

bsdcpio_test_SOURCES= \
	$(test_utils_SOURCES) \
	cpio/cmdline.c \
	cpio/test/test.h \
	cpio/test/test_0.c \
	cpio/test/test_basic.c \
	cpio/test/test_cmdline.c \
	cpio/test/test_extract_cpio_Z.c \
	cpio/test/test_extract_cpio_bz2.c \
	cpio/test/test_extract_cpio_grz.c \
	cpio/test/test_extract_cpio_gz.c \
	cpio/test/test_extract_cpio_lrz.c \
	cpio/test/test_extract_cpio_lz.c \
	cpio/test/test_extract_cpio_lz4.c \
	cpio/test/test_extract_cpio_lzma.c \
	cpio/test/test_extract_cpio_lzo.c \
	cpio/test/test_extract_cpio_xz.c \
	cpio/test/test_format_newc.c \
	cpio/test/test_gcpio_compat.c \
	cpio/test/test_missing_file.c \
	cpio/test/test_option_0.c \
	cpio/test/test_option_B_upper.c \
	cpio/test/test_option_C_upper.c \
	cpio/test/test_option_J_upper.c \
	cpio/test/test_option_L_upper.c \
	cpio/test/test_option_Z_upper.c \
	cpio/test/test_option_a.c \
	cpio/test/test_option_b64encode.c \
	cpio/test/test_option_c.c \
	cpio/test/test_option_d.c \
	cpio/test/test_option_f.c \
	cpio/test/test_option_grzip.c \
	cpio/test/test_option_help.c \
	cpio/test/test_option_l.c \
	cpio/test/test_option_lrzip.c \
	cpio/test/test_option_lz4.c \
	cpio/test/test_option_lzma.c \
	cpio/test/test_option_lzop.c \
	cpio/test/test_option_m.c \
	cpio/test/test_option_passphrase.c \
	cpio/test/test_option_t.c \
	cpio/test/test_option_u.c \
	cpio/test/test_option_uuencode.c \
	cpio/test/test_option_version.c \
	cpio/test/test_option_xz.c \
	cpio/test/test_option_y.c \
	cpio/test/test_option_z.c \
	cpio/test/test_owner_parse.c \
	cpio/test/test_passthrough_dotdot.c \
	cpio/test/test_passthrough_reverse.c

bsdcpio_test_CPPFLAGS= \
	-I$(top_srcdir)/libarchive -I$(top_srcdir)/libarchive_fe \
	-I$(top_srcdir)/test_utils \
	-I$(top_srcdir)/cpio -I$(top_srcdir)/cpio/test \
	-I$(top_builddir)/cpio/test \
	$(PLATFORMCPPFLAGS)
bsdcpio_test_LDADD=libarchive_fe.la

cpio/test/list.h: Makefile
	$(MKDIR_P) cpio/test
	cat $(top_srcdir)/cpio/test/test_*.c | grep '^DEFINE_TEST' > cpio/test/list.h

if BUILD_BSDCPIO
bsdcpio_test_programs= bsdcpio_test
bsdcpio_TESTS_ENVIRONMENT= BSDCPIO=`cd $(top_builddir);/bin/pwd`/bsdcpio$(EXEEXT) BSDCPIO_TEST_FILES=`cd $(top_srcdir);/bin/pwd`/cpio/test
else
bsdcpio_test_programs=
bsdcpio_TESTS_ENVIRONMENT=
endif

bsdcpio_test_EXTRA_DIST= \
	cpio/test/list.h \
	cpio/test/test_extract.cpio.Z.uu \
	cpio/test/test_extract.cpio.bz2.uu \
	cpio/test/test_extract.cpio.grz.uu \
	cpio/test/test_extract.cpio.gz.uu \
	cpio/test/test_extract.cpio.lrz.uu \
	cpio/test/test_extract.cpio.lz.uu \
	cpio/test/test_extract.cpio.lz4.uu \
	cpio/test/test_extract.cpio.lzma.uu \
	cpio/test/test_extract.cpio.lzo.uu \
	cpio/test/test_extract.cpio.xz.uu \
	cpio/test/test_gcpio_compat_ref.bin.uu \
	cpio/test/test_gcpio_compat_ref.crc.uu \
	cpio/test/test_gcpio_compat_ref.newc.uu \
	cpio/test/test_gcpio_compat_ref.ustar.uu \
	cpio/test/test_gcpio_compat_ref_nosym.bin.uu \
	cpio/test/test_gcpio_compat_ref_nosym.crc.uu \
	cpio/test/test_gcpio_compat_ref_nosym.newc.uu \
	cpio/test/test_gcpio_compat_ref_nosym.ustar.uu \
	cpio/test/test_option_f.cpio.uu \
	cpio/test/test_option_m.cpio.uu \
	cpio/test/test_option_passphrase.zip.uu \
	cpio/test/test_option_t.cpio.uu \
	cpio/test/test_option_t.stdout.uu \
	cpio/test/test_option_tv.stdout.uu \
	cpio/test/CMakeLists.txt

#
#
# bsdcat source, docs, etc.
#
#

bsdcat_SOURCES= \
		cat/bsdcat.c \
		cat/bsdcat.h \
		cat/bsdcat_platform.h \
		cat/cmdline.c

if INC_WINDOWS_FILES
bsdcat_SOURCES+=
endif

bsdcat_DEPENDENCIES = libarchive.la libarchive_fe.la


if STATIC_BSDCAT
bsdcat_ldstatic= -static
bsdcat_ccstatic= -DLIBARCHIVE_STATIC
else
bsdcat_ldstatic=
bsdcat_ccstatic=
endif

bsdcat_LDADD= libarchive_fe.la libarchive.la $(LTLIBICONV)
bsdcat_CPPFLAGS= -I$(top_srcdir)/libarchive -I$(top_srcdir)/libarchive_fe $(bsdcat_ccstatic) $(PLATFORMCPPFLAGS)
bsdcat_LDFLAGS= $(bsdcat_ldstatic)

bsdcat_EXTRA_DIST= \
	cat/bsdcat.1 \
	cat/CMakeLists.txt


if BUILD_BSDCAT
# Manpages to install
bsdcat_man_MANS= cat/bsdcat.1
bsdcat_programs= bsdcat
else
bsdcat_man_MANS=
bsdcat_programs=
endif

#
# bsdcat_test
#

bsdcat_test_SOURCES= \
	$(test_utils_SOURCES) \
	cat/test/test.h \
	cat/test/test_0.c \
	cat/test/test_empty_gz.c \
	cat/test/test_empty_lz4.c \
	cat/test/test_empty_xz.c \
	cat/test/test_error.c \
	cat/test/test_error_mixed.c \
	cat/test/test_expand_Z.c \
	cat/test/test_expand_bz2.c \
	cat/test/test_expand_gz.c \
	cat/test/test_expand_lz4.c \
	cat/test/test_expand_mixed.c \
	cat/test/test_expand_plain.c \
	cat/test/test_expand_xz.c \
	cat/test/test_help.c \
	cat/test/test_version.c

bsdcat_test_CPPFLAGS= \
	-I$(top_srcdir)/libarchive -I$(top_srcdir)/libarchive_fe \
	-I$(top_srcdir)/test_utils \
	-I$(top_srcdir)/cat -I$(top_srcdir)/cat/test \
	-I$(top_builddir)/cat/test \
	$(PLATFORMCPPFLAGS)
bsdcat_test_LDADD=libarchive_fe.la

cat/test/list.h: Makefile
	cat $(top_srcdir)/cat/test/test_*.c | grep '^DEFINE_TEST' > cat/test/list.h

if BUILD_BSDCAT
bsdcat_test_programs= bsdcat_test
bsdcat_TESTS_ENVIRONMENT= BSDCAT=`cd $(top_builddir);/bin/pwd`/bsdcat$(EXEEXT) BSDCAT_TEST_FILES=`cd $(top_srcdir);/bin/pwd`/cat/test
else
bsdcat_test_programs=
bsdcat_TESTS_ENVIRONMENT=
endif

bsdcat_test_EXTRA_DIST= \
	cat/test/list.h \
	cat/test/test_empty.gz.uu \
	cat/test/test_empty.lz4.uu \
	cat/test/test_empty.xz.uu \
	cat/test/test_expand.Z.uu \
	cat/test/test_expand.bz2.uu \
	cat/test/test_expand.gz.uu \
	cat/test/test_expand.lz4.uu \
	cat/test/test_expand.plain.uu \
	cat/test/test_expand.xz.uu \
	cat/test/CMakeLists.txt
