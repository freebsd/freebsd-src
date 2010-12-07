# $FreeBSD$

# Where to find the libarchive sources
LA_SRCDIR=${.CURDIR}/..

TESTS= \
	test_acl_basic.c			\
	test_acl_freebsd.c			\
	test_acl_pax.c				\
	test_archive_api_feature.c		\
	test_bad_fd.c				\
	test_compat_bzip2.c			\
	test_compat_cpio.c			\
	test_compat_gtar.c			\
	test_compat_gzip.c			\
	test_compat_lzma.c			\
	test_compat_solaris_tar_acl.c		\
	test_compat_tar_hardlink.c		\
	test_compat_xz.c			\
	test_compat_zip.c			\
	test_empty_write.c			\
	test_entry.c				\
	test_entry_strmode.c			\
	test_extattr_freebsd.c			\
	test_fuzz.c				\
	test_link_resolver.c			\
	test_open_fd.c				\
	test_open_file.c			\
	test_open_filename.c			\
	test_pax_filename_encoding.c		\
	test_read_compress_program.c		\
	test_read_data_large.c			\
	test_read_disk.c			\
	test_read_disk_entry_from_file.c	\
	test_read_extract.c			\
	test_read_file_nonexistent.c		\
	test_read_format_ar.c			\
	test_read_format_cpio_bin.c		\
	test_read_format_cpio_bin_Z.c		\
	test_read_format_cpio_bin_be.c		\
	test_read_format_cpio_bin_bz2.c		\
	test_read_format_cpio_bin_gz.c		\
	test_read_format_cpio_bin_lzma.c	\
	test_read_format_cpio_bin_xz.c		\
	test_read_format_cpio_odc.c		\
	test_read_format_cpio_svr4_gzip.c	\
	test_read_format_cpio_svr4c_Z.c		\
	test_read_format_empty.c		\
	test_read_format_gtar_gz.c		\
	test_read_format_gtar_lzma.c		\
	test_read_format_gtar_sparse.c		\
	test_read_format_iso_gz.c		\
	test_read_format_iso_multi_extent.c	\
	test_read_format_isorr_rr_moved.c	\
	test_read_format_isojoliet_bz2.c	\
	test_read_format_isojoliet_long.c	\
	test_read_format_isojoliet_rr.c		\
	test_read_format_isorr_bz2.c		\
	test_read_format_isorr_ce.c		\
	test_read_format_isorr_new_bz2.c	\
	test_read_format_isozisofs_bz2.c	\
	test_read_format_mtree.c		\
	test_read_format_pax_bz2.c		\
	test_read_format_raw.c			\
	test_read_format_tar.c			\
	test_read_format_tar_empty_filename.c	\
	test_read_format_tbz.c			\
	test_read_format_tgz.c			\
	test_read_format_tlz.c			\
	test_read_format_txz.c			\
	test_read_format_tz.c			\
	test_read_format_zip.c			\
	test_read_large.c			\
	test_read_pax_truncated.c		\
	test_read_position.c			\
	test_read_truncated.c			\
	test_read_uu.c				\
	test_tar_filenames.c			\
	test_tar_large.c			\
	test_ustar_filenames.c			\
	test_write_compress.c			\
	test_write_compress_bzip2.c		\
	test_write_compress_gzip.c		\
	test_write_compress_lzma.c		\
	test_write_compress_program.c		\
	test_write_compress_xz.c		\
	test_write_disk.c			\
	test_write_disk_failures.c		\
	test_write_disk_hardlink.c		\
	test_write_disk_perms.c			\
	test_write_disk_secure.c		\
	test_write_disk_sparse.c		\
	test_write_disk_symlink.c		\
	test_write_disk_times.c			\
	test_write_format_ar.c			\
	test_write_format_cpio.c		\
	test_write_format_cpio_empty.c		\
	test_write_format_cpio_newc.c		\
	test_write_format_cpio_odc.c		\
	test_write_format_mtree.c		\
	test_write_format_pax.c			\
	test_write_format_shar_empty.c		\
	test_write_format_tar.c			\
	test_write_format_tar_empty.c		\
	test_write_format_tar_ustar.c		\
	test_write_format_zip.c			\
	test_write_format_zip_empty.c		\
	test_write_format_zip_no_compression.c	\
	test_write_open_memory.c


# Build the test program.
SRCS= \
	${TESTS}				\
	list.h					\
	main.c					\
	read_open_memory.c

NO_MAN=yes

PROG=libarchive_test
INTERNALPROG=yes  # Don't install this; it's just for testing
DPADD=${LIBBZ2} ${LIBZ} ${LIBMD} ${LIBCRYPTO} ${LIBBSDXML}
LDADD= -L ${.OBJDIR}/.. -larchive
LDADD+= -lz -lbz2 -llzma -lmd -lcrypto -lbsdxml
CFLAGS+= -g
CFLAGS+= -I${LA_SRCDIR} -I.
CFLAGS+= -DHAVE_LIBLZMA=1 -DHAVE_LZMA_H=1

# Uncomment to link against dmalloc
#LDADD+= -L/usr/local/lib -ldmalloc
#CFLAGS+= -I/usr/local/include -DUSE_DMALLOC

# Build libarchive_test and run it.
check test: libarchive_test
	./libarchive_test -r ${.CURDIR}

# list.h is just a list of all tests, as indicated by DEFINE_TEST macro lines
list.h: ${TESTS} Makefile
	(cd ${.CURDIR}; cat test_*.c) | grep DEFINE_TEST > list.h

CLEANFILES += *.out *.o *.core *~ list.h .dirstamp .depend
CLEANDIRS += .deps .libs

cleantest:
	-chmod -R +w /tmp/libarchive_test.*
	rm -rf /tmp/libarchive_test.*

.include <bsd.prog.mk>
