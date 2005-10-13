# $FreeBSD$

lib_LTLIBRARIES=	libarchive.la

include_HEADERS= archive_entry.h
# Note: archive.h is built from archive.h.in, so don't
# include archive.h in the distfile, but do install it.
nodist_include_HEADERS= archive.h

libarchive_la_SOURCES=					\
	archive_check_magic.c				\
	archive_entry.c					\
	archive_entry.h					\
	archive_platform.h				\
	archive_private.h				\
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
	archive_read_support_format_iso9660.c		\
	archive_read_support_format_tar.c		\
	archive_read_support_format_zip.c		\
	archive_string.c				\
	archive_string.h				\
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

libarchive_la_LIBADD=-lz -lbz2
libarchive_la_LDFLAGS= -version-info $(LIBTOOL_VERSION)

dist_man_MANS=	archive_entry.3				\
	archive_read.3					\
	archive_util.3					\
	archive_write.3					\
	libarchive.3					\
	libarchive-formats.5				\
	tar.5

distclean-local:
	-rm -rf autom4te.cache/
	-rm -f *~

maintainer-clean-local:
	make -f Makefile.freebsd maintainer-clean
