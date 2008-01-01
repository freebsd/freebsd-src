# $FreeBSD$

# Where to find the libarchive sources
LA_SRCDIR=${.CURDIR}/..
.PATH: ${LA_SRCDIR}

# Get a list of all libarchive source files
LA_SRCS!=make -f ${LA_SRCDIR}/Makefile -V SRCS

TESTFILES= \
	test_compat_gtar_1.tgz					\
	test_compat_zip_1.zip					\
	test_read_format_gtar_sparse_1_13.tgz			\
	test_read_format_gtar_sparse_1_17.tgz			\
	test_read_format_gtar_sparse_1_17_posix00.tgz		\
	test_read_format_gtar_sparse_1_17_posix01.tgz		\
	test_read_format_gtar_sparse_1_17_posix10.tgz		\
	test_read_format_gtar_sparse_1_17_posix10_modified.tar

TESTS= \
	test_acl_basic.c			\
	test_acl_pax.c				\
	test_archive_api_feature.c		\
	test_bad_fd.c				\
	test_compat_gtar.c			\
	test_compat_zip.c			\
	test_empty_write.c			\
	test_entry.c				\
	test_entry_strmode.c			\
	test_read_compress_program.c		\
	test_read_data_large.c			\
	test_read_extract.c			\
	test_read_format_ar.c			\
	test_read_format_cpio_bin.c		\
	test_read_format_cpio_bin_Z.c		\
	test_read_format_cpio_bin_bz2.c		\
	test_read_format_cpio_bin_gz.c		\
	test_read_format_cpio_odc.c		\
	test_read_format_cpio_svr4_gzip.c	\
	test_read_format_cpio_svr4c_Z.c		\
	test_read_format_empty.c		\
	test_read_format_gtar_gz.c		\
	test_read_format_gtar_sparse.c		\
	test_read_format_iso_gz.c		\
	test_read_format_isorr_bz2.c		\
	test_read_format_mtree.c		\
	test_read_format_pax_bz2.c		\
	test_read_format_tar.c			\
	test_read_format_tbz.c			\
	test_read_format_tgz.c			\
	test_read_format_tz.c			\
	test_read_format_zip.c			\
	test_read_large.c			\
	test_read_pax_truncated.c		\
	test_read_position.c			\
	test_read_truncated.c			\
	test_tar_filenames.c			\
	test_tar_large.c			\
	test_write_compress_program.c		\
	test_write_disk.c			\
	test_write_disk_perms.c			\
	test_write_disk_secure.c		\
	test_write_format_ar.c			\
	test_write_format_cpio.c		\
	test_write_format_cpio_odc.c		\
	test_write_format_cpio_newc.c		\
	test_write_format_cpio_empty.c		\
	test_write_format_shar_empty.c		\
	test_write_format_tar.c			\
	test_write_format_tar_empty.c		\
	test_write_open_memory.c


# Build the test program using all libarchive sources + the test sources.
SRCS= ${LA_SRCS}				\
	${TESTS}				\
	list.h					\
	main.c					\
	read_open_memory.c

CLEANFILES+= list.h archive.h

NO_MAN=yes

PROG=libarchive_test
INTERNALPROG=yes  # Don't install this; it's just for testing
DPADD=${LIBBZ2} ${LIBZ}
CFLAGS+= -DPLATFORM_CONFIG_H=\"config_freebsd.h\"
LDADD= -lz -lbz2
CFLAGS+= -static -g
CFLAGS+= -I${.OBJDIR}
CFLAGS+= -I${.CURDIR}
CFLAGS+= -I${LA_SRCDIR}
# Without this, libarchive source files find archive.h in LA_SRCDIR,
# which may not be the same as archive.h in the test dir.
CFLAGS+= -I-

# Uncomment to link against dmalloc
LDADD+= -L/usr/local/lib -ldmalloc
CFLAGS+= -I/usr/local/include -DUSE_DMALLOC
WARNS=6

# Build libarchive_test and run it.
check test: libarchive_test ${TESTFILES}
	./libarchive_test

.for f in ${TESTFILES}
${f}: ${f}.uu
	uudecode -p ${.CURDIR}/${f}.uu >${f}
.endfor

INCS=archive.h list.h

# Build archive.h, but in our .OBJDIR, not libarchive's
# This keeps libarchive_test and libarchive builds completely separate.
archive.h: ${LA_SRCDIR}/archive.h.in ${LA_SRCDIR}/Makefile
	cd ${LA_SRCDIR} && unset MAKEOBJDIRPREFIX && MAKEOBJDIR=${.OBJDIR} make archive.h

# list.h is just a list of all tests, as indicated by DEFINE_TEST macro lines
list.h: ${TESTS} Makefile
	(cd ${.CURDIR}; cat ${TESTS}) | grep DEFINE_TEST > list.h

CLEANFILES += *.out *.o *.core *~ list.h archive.h ${TESTFILES}

cleantest:
	-chmod -R +w /tmp/libarchive_test.*
	rm -rf /tmp/libarchive_test.*

.include <bsd.prog.mk>
