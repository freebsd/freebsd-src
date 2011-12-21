## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS= foreign subdir-objects
ACLOCAL_AMFLAGS = -I build/autoconf

#
# What to build and install
#
lib_LTLIBRARIES=	libarchive.la
noinst_LTLIBRARIES=	libarchive_fe.la
bin_PROGRAMS=	$(bsdtar_programs) $(bsdcpio_programs)
man_MANS= $(libarchive_man_MANS) $(bsdtar_man_MANS) $(bsdcpio_man_MANS)
BUILT_SOURCES= libarchive/test/list.h tar/test/list.h cpio/test/list.h

#
# What to test: We always test libarchive, test bsdtar and bsdcpio only
# if we built them.
#
check_PROGRAMS= libarchive_test $(bsdtar_test_programs) $(bsdcpio_test_programs)
TESTS= libarchive_test $(bsdtar_test_programs) $(bsdcpio_test_programs)
TESTS_ENVIRONMENT= $(libarchive_TESTS_ENVIRONMENT) $(bsdtar_TESTS_ENVIRONMENT) $(bsdcpio_TESTS_ENVIRONMENT)
# Always build and test both bsdtar and bsdcpio as part of 'distcheck'
DISTCHECK_CONFIGURE_FLAGS = --enable-bsdtar --enable-bsdcpio
# Uncommenting this line can help diagnose some errors.  This is ordinarily
# enabled in the libarchive development branch but is disabled
# for libarchive production releases.
#AM_CFLAGS=-Wall -Werror
PLATFORMCPPFLAGS = @PLATFORMCPPFLAGS@
AM_CPPFLAGS=$(PLATFORMCPPFLAGS)

#
# What to include in the distribution
#
EXTRA_DIST= \
	CMakeLists.txt			\
	build/autogen.sh		\
	build/bump-version.sh		\
	build/clean.sh			\
	build/cmake			\
	build/version			\
	contrib				\
	doc				\
	examples			\
	$(libarchive_EXTRA_DIST)	\
	$(libarchive_test_EXTRA_DIST)	\
	$(bsdtar_EXTRA_DIST)		\
	$(bsdtar_test_EXTRA_DIST)	\
	$(bsdcpio_EXTRA_DIST)		\
	$(bsdcpio_test_EXTRA_DIST)

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

# Verify cmake builds as part of the acceptance
distcheck-hook:
	mkdir $(distdir)/_build/cmtest
	cd $(distdir)/_build/cmtest && cmake ../.. && make && make test
	rm -rf $(distdir)/_build/cmtest

#
# Extra rules for cleanup
#
DISTCLEANFILES=					\
	libarchive/test/list.h			\
	tar/test/list.h				\
	cpio/test/list.h

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

#
# Libarchive headers, source, etc.
#
#

include_HEADERS= libarchive/archive.h libarchive/archive_entry.h

libarchive_la_SOURCES=						\
	libarchive/archive_check_magic.c			\
	libarchive/archive_crc32.h				\
	libarchive/archive_endian.h				\
	libarchive/archive_entry.c				\
	libarchive/archive_entry.h				\
	libarchive/archive_entry_copy_stat.c			\
	libarchive/archive_entry_link_resolver.c		\
	libarchive/archive_entry_private.h			\
	libarchive/archive_entry_stat.c				\
	libarchive/archive_entry_strmode.c			\
	libarchive/archive_entry_xattr.c			\
	libarchive/archive_hash.h				\
	libarchive/archive_platform.h				\
	libarchive/archive_private.h				\
	libarchive/archive_read.c				\
	libarchive/archive_read_data_into_fd.c			\
	libarchive/archive_read_disk.c				\
	libarchive/archive_read_disk_entry_from_file.c		\
	libarchive/archive_read_disk_private.h			\
	libarchive/archive_read_disk_set_standard_lookup.c	\
	libarchive/archive_read_extract.c			\
	libarchive/archive_read_open_fd.c			\
	libarchive/archive_read_open_file.c			\
	libarchive/archive_read_open_filename.c			\
	libarchive/archive_read_open_memory.c			\
	libarchive/archive_read_private.h			\
	libarchive/archive_read_support_compression_all.c	\
	libarchive/archive_read_support_compression_bzip2.c	\
	libarchive/archive_read_support_compression_compress.c	\
	libarchive/archive_read_support_compression_gzip.c	\
	libarchive/archive_read_support_compression_none.c	\
	libarchive/archive_read_support_compression_program.c	\
	libarchive/archive_read_support_compression_rpm.c	\
	libarchive/archive_read_support_compression_uu.c	\
	libarchive/archive_read_support_compression_xz.c	\
	libarchive/archive_read_support_format_all.c		\
	libarchive/archive_read_support_format_ar.c		\
	libarchive/archive_read_support_format_cpio.c		\
	libarchive/archive_read_support_format_empty.c		\
	libarchive/archive_read_support_format_iso9660.c	\
	libarchive/archive_read_support_format_mtree.c		\
	libarchive/archive_read_support_format_raw.c		\
	libarchive/archive_read_support_format_tar.c		\
	libarchive/archive_read_support_format_xar.c		\
	libarchive/archive_read_support_format_zip.c		\
	libarchive/archive_string.c				\
	libarchive/archive_string.h				\
	libarchive/archive_string_sprintf.c			\
	libarchive/archive_util.c				\
	libarchive/archive_virtual.c				\
	libarchive/archive_write.c				\
	libarchive/archive_write_disk.c				\
	libarchive/archive_write_disk_private.h			\
	libarchive/archive_write_disk_set_standard_lookup.c	\
	libarchive/archive_write_open_fd.c			\
	libarchive/archive_write_open_file.c			\
	libarchive/archive_write_open_filename.c		\
	libarchive/archive_write_open_memory.c			\
	libarchive/archive_write_private.h			\
	libarchive/archive_write_set_compression_bzip2.c	\
	libarchive/archive_write_set_compression_compress.c	\
	libarchive/archive_write_set_compression_gzip.c		\
	libarchive/archive_write_set_compression_none.c		\
	libarchive/archive_write_set_compression_program.c	\
	libarchive/archive_write_set_compression_xz.c		\
	libarchive/archive_write_set_format.c			\
	libarchive/archive_write_set_format_ar.c		\
	libarchive/archive_write_set_format_by_name.c		\
	libarchive/archive_write_set_format_cpio.c		\
	libarchive/archive_write_set_format_cpio_newc.c		\
	libarchive/archive_write_set_format_mtree.c		\
	libarchive/archive_write_set_format_pax.c		\
	libarchive/archive_write_set_format_shar.c		\
	libarchive/archive_write_set_format_ustar.c		\
	libarchive/archive_write_set_format_zip.c		\
	libarchive/config_freebsd.h				\
	libarchive/filter_fork.c				\
	libarchive/filter_fork.h

if INC_WINDOWS_FILES
libarchive_la_SOURCES+=						\
	libarchive/archive_entry_copy_bhfi.c			\
	libarchive/archive_windows.h				\
	libarchive/archive_windows.c				\
	libarchive/filter_fork_windows.c
endif

# -no-undefined marks that libarchive doesn't rely on symbols
# defined in the application.  This is mandatory for cygwin.
libarchive_la_LDFLAGS= -no-undefined -version-info $(ARCHIVE_LIBTOOL_VERSION)

# Manpages to install
libarchive_man_MANS=						\
	libarchive/archive_entry.3				\
	libarchive/archive_read.3				\
	libarchive/archive_read_disk.3				\
	libarchive/archive_util.3				\
	libarchive/archive_write.3				\
	libarchive/archive_write_disk.3				\
	libarchive/cpio.5					\
	libarchive/libarchive.3					\
	libarchive/libarchive_internals.3			\
	libarchive/libarchive-formats.5				\
	libarchive/mtree.5					\
	libarchive/tar.5

# Additional libarchive files to include in the distribution
libarchive_EXTRA_DIST=			\
	libarchive/test/list.h		\
	libarchive/archive_windows.c	\
	libarchive/archive_windows.h	\
	libarchive/filter_fork_windows.c	\
	libarchive/CMakeLists.txt	\
	$(libarchive_man_MANS)

# pkgconfig
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = build/pkgconfig/libarchive.pc

#
#
# libarchive_test program
#
#
libarchive_test_SOURCES=					\
	$(libarchive_la_SOURCES)				\
	libarchive/test/main.c					\
	libarchive/test/read_open_memory.c			\
	libarchive/test/test.h					\
	libarchive/test/test_acl_basic.c			\
	libarchive/test/test_acl_freebsd.c			\
	libarchive/test/test_acl_pax.c				\
	libarchive/test/test_archive_api_feature.c		\
	libarchive/test/test_bad_fd.c				\
	libarchive/test/test_compat_bzip2.c			\
	libarchive/test/test_compat_cpio.c			\
	libarchive/test/test_compat_gtar.c			\
	libarchive/test/test_compat_gzip.c			\
	libarchive/test/test_compat_lzma.c			\
	libarchive/test/test_compat_solaris_tar_acl.c		\
	libarchive/test/test_compat_tar_hardlink.c		\
	libarchive/test/test_compat_xz.c			\
	libarchive/test/test_compat_zip.c			\
	libarchive/test/test_empty_write.c			\
	libarchive/test/test_entry.c				\
	libarchive/test/test_extattr_freebsd.c			\
	libarchive/test/test_fuzz.c				\
	libarchive/test/test_entry_strmode.c			\
	libarchive/test/test_link_resolver.c			\
	libarchive/test/test_open_failure.c			\
	libarchive/test/test_open_fd.c				\
	libarchive/test/test_open_file.c			\
	libarchive/test/test_open_filename.c			\
	libarchive/test/test_pax_filename_encoding.c		\
	libarchive/test/test_read_compress_program.c		\
	libarchive/test/test_read_data_large.c			\
	libarchive/test/test_read_disk.c			\
	libarchive/test/test_read_disk_entry_from_file.c	\
	libarchive/test/test_read_extract.c			\
	libarchive/test/test_read_file_nonexistent.c		\
	libarchive/test/test_read_format_ar.c			\
	libarchive/test/test_read_format_cpio_bin.c		\
	libarchive/test/test_read_format_cpio_bin_Z.c		\
	libarchive/test/test_read_format_cpio_bin_be.c		\
	libarchive/test/test_read_format_cpio_bin_bz2.c		\
	libarchive/test/test_read_format_cpio_bin_gz.c		\
	libarchive/test/test_read_format_cpio_bin_lzma.c	\
	libarchive/test/test_read_format_cpio_bin_xz.c		\
	libarchive/test/test_read_format_cpio_odc.c		\
	libarchive/test/test_read_format_cpio_svr4_bzip2_rpm.c	\
	libarchive/test/test_read_format_cpio_svr4_gzip.c	\
	libarchive/test/test_read_format_cpio_svr4_gzip_rpm.c	\
	libarchive/test/test_read_format_cpio_svr4c_Z.c		\
	libarchive/test/test_read_format_empty.c		\
	libarchive/test/test_read_format_gtar_gz.c		\
	libarchive/test/test_read_format_gtar_lzma.c		\
	libarchive/test/test_read_format_gtar_sparse.c		\
	libarchive/test/test_read_format_iso_Z.c		\
	libarchive/test/test_read_format_iso_multi_extent.c	\
	libarchive/test/test_read_format_isojoliet_bz2.c	\
	libarchive/test/test_read_format_isojoliet_long.c	\
	libarchive/test/test_read_format_isojoliet_rr.c		\
	libarchive/test/test_read_format_isorr_bz2.c		\
	libarchive/test/test_read_format_isorr_ce.c		\
	libarchive/test/test_read_format_isorr_new_bz2.c	\
	libarchive/test/test_read_format_isorr_rr_moved.c	\
	libarchive/test/test_read_format_isozisofs_bz2.c	\
	libarchive/test/test_read_format_mtree.c		\
	libarchive/test/test_read_format_pax_bz2.c		\
	libarchive/test/test_read_format_raw.c			\
	libarchive/test/test_read_format_tar.c			\
	libarchive/test/test_read_format_tar_empty_filename.c	\
	libarchive/test/test_read_format_tbz.c			\
	libarchive/test/test_read_format_tgz.c			\
	libarchive/test/test_read_format_tlz.c			\
	libarchive/test/test_read_format_txz.c			\
	libarchive/test/test_read_format_tz.c			\
	libarchive/test/test_read_format_xar.c			\
	libarchive/test/test_read_format_zip.c			\
	libarchive/test/test_read_large.c			\
	libarchive/test/test_read_pax_truncated.c		\
	libarchive/test/test_read_position.c			\
	libarchive/test/test_read_truncated.c			\
	libarchive/test/test_read_uu.c				\
	libarchive/test/test_tar_filenames.c			\
	libarchive/test/test_tar_large.c			\
	libarchive/test/test_ustar_filenames.c			\
	libarchive/test/test_write_compress.c			\
	libarchive/test/test_write_compress_bzip2.c		\
	libarchive/test/test_write_compress_gzip.c		\
	libarchive/test/test_write_compress_lzma.c		\
	libarchive/test/test_write_compress_program.c		\
	libarchive/test/test_write_compress_xz.c		\
	libarchive/test/test_write_disk.c			\
	libarchive/test/test_write_disk_failures.c		\
	libarchive/test/test_write_disk_hardlink.c		\
	libarchive/test/test_write_disk_perms.c			\
	libarchive/test/test_write_disk_secure.c		\
	libarchive/test/test_write_disk_sparse.c		\
	libarchive/test/test_write_disk_symlink.c		\
	libarchive/test/test_write_disk_times.c			\
	libarchive/test/test_write_format_ar.c			\
	libarchive/test/test_write_format_cpio.c		\
	libarchive/test/test_write_format_cpio_empty.c		\
	libarchive/test/test_write_format_cpio_odc.c		\
	libarchive/test/test_write_format_cpio_newc.c		\
	libarchive/test/test_write_format_mtree.c		\
	libarchive/test/test_write_format_pax.c			\
	libarchive/test/test_write_format_shar_empty.c		\
	libarchive/test/test_write_format_tar.c			\
	libarchive/test/test_write_format_tar_empty.c		\
	libarchive/test/test_write_format_tar_ustar.c		\
	libarchive/test/test_write_format_zip.c			\
	libarchive/test/test_write_format_zip_empty.c		\
	libarchive/test/test_write_format_zip_no_compression.c	\
	libarchive/test/test_write_open_memory.c

libarchive_test_CPPFLAGS= -I$(top_srcdir)/libarchive -I$(top_builddir)/libarchive/test -DLIBARCHIVE_STATIC $(PLATFORMCPPFLAGS)

# The "list.h" file just lists all of the tests defined in all of the sources.
# Building it automatically provides a sanity-check on libarchive_test_SOURCES
# above.
libarchive/test/list.h: Makefile
	cat $(top_srcdir)/libarchive/test/test_*.c | grep DEFINE_TEST > libarchive/test/list.h

libarchive_TESTS_ENVIRONMENT= LIBARCHIVE_TEST_FILES=`cd $(top_srcdir);/bin/pwd`/libarchive/test

libarchive_test_EXTRA_DIST=\
	libarchive/test/test_compat_bzip2_1.tbz.uu			\
	libarchive/test/test_compat_bzip2_2.tbz.uu			\
	libarchive/test/test_compat_cpio_1.cpio.uu			\
	libarchive/test/test_compat_gtar_1.tar.uu			\
	libarchive/test/test_compat_gzip_1.tgz.uu			\
	libarchive/test/test_compat_gzip_2.tgz.uu			\
	libarchive/test/test_compat_lzma_1.tlz.uu			\
	libarchive/test/test_compat_lzma_2.tlz.uu			\
	libarchive/test/test_compat_lzma_3.tlz.uu			\
	libarchive/test/test_compat_solaris_tar_acl.tar.uu		\
	libarchive/test/test_compat_tar_hardlink_1.tar.uu		\
	libarchive/test/test_compat_xz_1.txz.uu				\
	libarchive/test/test_compat_zip_1.zip.uu			\
	libarchive/test/test_fuzz_1.iso.Z.uu				\
	libarchive/test/test_pax_filename_encoding.tar.uu		\
	libarchive/test/test_read_format_ar.ar.uu			\
	libarchive/test/test_read_format_cpio_bin_be.cpio.uu		\
	libarchive/test/test_read_format_cpio_svr4_bzip2_rpm.rpm.uu	\
	libarchive/test/test_read_format_cpio_svr4_gzip_rpm.rpm.uu	\
	libarchive/test/test_read_format_gtar_sparse_1_13.tar.uu	\
	libarchive/test/test_read_format_gtar_sparse_1_17.tar.uu	\
	libarchive/test/test_read_format_gtar_sparse_1_17_posix00.tar.uu \
	libarchive/test/test_read_format_gtar_sparse_1_17_posix01.tar.uu \
	libarchive/test/test_read_format_gtar_sparse_1_17_posix10.tar.uu \
	libarchive/test/test_read_format_gtar_sparse_1_17_posix10_modified.tar.uu \
	libarchive/test/test_read_format_iso.iso.Z.uu			\
	libarchive/test/test_read_format_iso_2.iso.Z.uu			\
	libarchive/test/test_read_format_iso_joliet.iso.Z.uu		\
	libarchive/test/test_read_format_iso_joliet_long.iso.Z.uu	\
	libarchive/test/test_read_format_iso_joliet_rockridge.iso.Z.uu	\
	libarchive/test/test_read_format_iso_multi_extent.iso.Z.uu	\
	libarchive/test/test_read_format_iso_rockridge.iso.Z.uu		\
	libarchive/test/test_read_format_iso_rockridge_ce.iso.Z.uu	\
	libarchive/test/test_read_format_iso_rockridge_new.iso.Z.uu	\
	libarchive/test/test_read_format_iso_rockridge_rr_moved.iso.Z.uu\
	libarchive/test/test_read_format_iso_zisofs.iso.Z.uu		\
	libarchive/test/test_read_format_mtree.mtree.uu			\
	libarchive/test/test_read_format_raw.data.Z.uu			\
	libarchive/test/test_read_format_raw.data.uu			\
	libarchive/test/test_read_format_tar_empty_filename.tar.uu	\
	libarchive/test/test_read_format_zip.zip.uu			\
	libarchive/test/CMakeLists.txt					\
	libarchive/test/README

#
# Common code for libarchive frontends (cpio, tar)
#
libarchive_fe_la_SOURCES=			\
	libarchive_fe/err.c			\
	libarchive_fe/err.h			\
	libarchive_fe/lafe_platform.h		\
	libarchive_fe/line_reader.c		\
	libarchive_fe/line_reader.h		\
	libarchive_fe/matching.c		\
	libarchive_fe/matching.h		\
	libarchive_fe/pathmatch.c		\
	libarchive_fe/pathmatch.h

#
#
# bsdtar source, docs, etc.
#
#

bsdtar_SOURCES=				\
		tar/bsdtar.c		\
		tar/bsdtar.h		\
		tar/bsdtar_platform.h	\
		tar/cmdline.c		\
		tar/getdate.c		\
		tar/read.c		\
		tar/subst.c		\
		tar/tree.c		\
		tar/tree.h		\
		tar/util.c		\
		tar/write.c

if INC_WINDOWS_FILES
bsdtar_SOURCES+=			\
		tar/bsdtar_windows.h	\
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

bsdtar_LDADD= libarchive.la libarchive_fe.la
bsdtar_CPPFLAGS= -I$(top_srcdir)/libarchive -I$(top_srcdir)/libarchive_fe $(bsdtar_ccstatic) $(PLATFORMCPPFLAGS)
bsdtar_LDFLAGS= $(bsdtar_ldstatic)

bsdtar_EXTRA_DIST=		\
	tar/bsdtar.1		\
	tar/bsdtar_windows.h	\
	tar/bsdtar_windows.c	\
	tar/CMakeLists.txt	\
	tar/config_freebsd.h	\
	tar/test/list.h


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

bsdtar_test_SOURCES=						\
	tar/getdate.c						\
	tar/test/main.c						\
	tar/test/test.h						\
	tar/test/test_0.c					\
	tar/test/test_basic.c					\
	tar/test/test_copy.c					\
	tar/test/test_empty_mtree.c				\
	tar/test/test_getdate.c					\
	tar/test/test_help.c					\
	tar/test/test_option_T_upper.c				\
	tar/test/test_option_q.c				\
	tar/test/test_option_r.c				\
	tar/test/test_option_s.c				\
	tar/test/test_patterns.c				\
	tar/test/test_stdio.c					\
	tar/test/test_strip_components.c			\
	tar/test/test_symlink_dir.c				\
	tar/test/test_version.c					\
	tar/test/test_windows.c

# For now, bsdtar_test uses Windows shims from tar/bsdtar_windows.*
if INC_WINDOWS_FILES
bsdtar_test_SOURCES+=			\
		tar/bsdtar_windows.h	\
		tar/bsdtar_windows.c
endif

bsdtar_test_CPPFLAGS=\
	-I$(top_srcdir)/libarchive -I$(top_srcdir)/libarchive_fe \
	-I$(top_srcdir)/tar -I$(top_builddir)/tar/test \
	$(PLATFORMCPPFLAGS)

tar/test/list.h: Makefile
	cat $(top_srcdir)/tar/test/test_*.c | grep DEFINE_TEST > tar/test/list.h

if BUILD_BSDTAR
bsdtar_test_programs= bsdtar_test
bsdtar_TESTS_ENVIRONMENT= BSDTAR=`cd $(top_builddir);/bin/pwd`/bsdtar$(EXEEXT) BSDTAR_TEST_FILES=`cd $(top_srcdir);/bin/pwd`/tar/test
else
bsdtar_test_programs=
bsdtar_TESTS_ENVIRONMENT=
endif

bsdtar_test_EXTRA_DIST=			\
	tar/test/test_patterns_2.tar.uu	\
	tar/test/test_patterns_3.tar.uu	\
	tar/test/test_patterns_4.tar.uu	\
	tar/test/CMakeLists.txt


#
#
# bsdcpio source, docs, etc.
#
#

bsdcpio_SOURCES=			\
		cpio/cmdline.c		\
		cpio/cpio.c		\
		cpio/cpio.h		\
		cpio/cpio_platform.h

if INC_WINDOWS_FILES
bsdcpio_SOURCES+=			\
		cpio/cpio_windows.h	\
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

bsdcpio_LDADD= libarchive_fe.la libarchive.la
bsdcpio_CPPFLAGS= -I$(top_srcdir)/libarchive -I$(top_srcdir)/libarchive_fe $(bsdcpio_ccstatic) $(PLATFORMCPPFLAGS)
bsdcpio_LDFLAGS= $(bsdcpio_ldstatic)

bsdcpio_EXTRA_DIST=		\
	cpio/test/list.h	\
	cpio/bsdcpio.1		\
	cpio/cpio_windows.h	\
	cpio/cpio_windows.c	\
	cpio/CMakeLists.txt	\
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

bsdcpio_test_SOURCES=						\
	cpio/cmdline.c						\
	cpio/test/main.c					\
	cpio/test/test.h					\
	cpio/test/test_0.c					\
	cpio/test/test_basic.c					\
	cpio/test/test_cmdline.c				\
	cpio/test/test_format_newc.c				\
	cpio/test/test_gcpio_compat.c				\
	cpio/test/test_option_B_upper.c				\
	cpio/test/test_option_C_upper.c				\
	cpio/test/test_option_J_upper.c				\
	cpio/test/test_option_L_upper.c				\
	cpio/test/test_option_Z_upper.c				\
	cpio/test/test_option_a.c				\
	cpio/test/test_option_c.c				\
	cpio/test/test_option_d.c				\
	cpio/test/test_option_f.c				\
	cpio/test/test_option_help.c				\
	cpio/test/test_option_l.c				\
	cpio/test/test_option_lzma.c				\
	cpio/test/test_option_m.c				\
	cpio/test/test_option_t.c				\
	cpio/test/test_option_u.c				\
	cpio/test/test_option_version.c				\
	cpio/test/test_option_y.c				\
	cpio/test/test_option_z.c				\
	cpio/test/test_owner_parse.c				\
	cpio/test/test_passthrough_dotdot.c			\
	cpio/test/test_passthrough_reverse.c			\
	cpio/test/test_pathmatch.c

bsdcpio_test_CPPFLAGS= \
	-I$(top_srcdir)/libarchive -I$(top_srcdir)/libarchive_fe \
	-I$(top_srcdir)/cpio -I$(top_builddir)/cpio/test \
	$(PLATFORMCPPFLAGS)
bsdcpio_test_LDADD=libarchive_fe.la

cpio/test/list.h: Makefile
	cat $(top_srcdir)/cpio/test/test_*.c | grep DEFINE_TEST > cpio/test/list.h

if BUILD_BSDCPIO
bsdcpio_test_programs= bsdcpio_test
bsdcpio_TESTS_ENVIRONMENT= BSDCPIO=`cd $(top_builddir);/bin/pwd`/bsdcpio$(EXEEXT) BSDCPIO_TEST_FILES=`cd $(top_srcdir);/bin/pwd`/cpio/test
else
bsdcpio_test_programs=
bsdcpio_TESTS_ENVIRONMENT=
endif

bsdcpio_test_EXTRA_DIST=			\
	cpio/test/test_gcpio_compat_ref.bin.uu	\
	cpio/test/test_gcpio_compat_ref.crc.uu	\
	cpio/test/test_gcpio_compat_ref.newc.uu	\
	cpio/test/test_gcpio_compat_ref.ustar.uu \
	cpio/test/test_gcpio_compat_ref_nosym.bin.uu \
	cpio/test/test_gcpio_compat_ref_nosym.crc.uu \
	cpio/test/test_gcpio_compat_ref_nosym.newc.uu \
	cpio/test/test_gcpio_compat_ref_nosym.ustar.uu \
	cpio/test/test_option_f.cpio.uu		\
	cpio/test/test_option_m.cpio.uu		\
	cpio/test/test_option_t.cpio.uu		\
	cpio/test/test_option_t.stdout.uu	\
	cpio/test/test_option_tv.stdout.uu	\
	cpio/test/CMakeLists.txt
