# $FreeBSD$

TESTS= \
	test_acl_basic.c			\
	test_acl_pax.c				\
	test_archive_api_feature.c		\
	test_bad_fd.c				\
	test_entry.c				\
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
	test_write_compress_program.c		\
	test_write_disk.c			\
	test_write_disk_perms.c			\
	test_write_disk_secure.c		\
	test_write_format_ar.c			\
	test_write_format_cpio.c		\
	test_write_format_cpio_empty.c		\
	test_write_format_shar_empty.c		\
	test_write_format_tar.c			\
	test_write_format_tar_empty.c		\
	test_write_open_memory.c

SRCS= ${TESTS}					\
	list.h					\
	main.c					\
	read_open_memory.c

CLEANFILES+= list.h

NO_MAN=yes

PROG=libarchive_test
DPADD=${LIBARCHIVE} ${LIBBZ2} ${LIBZ}
LDADD= -larchive -lz -lbz2
CFLAGS+= -static -g
CFLAGS+= -I${.OBJDIR}

# Uncomment to link against dmalloc
#LDADD+= -L/usr/local/lib -ldmalloc
#CFLAGS+= -I/usr/local/include -DUSE_DMALLOC
#WARNS=6

test:	libarchive_test
	./libarchive_test -k

list.h: ${TESTS} Makefile
	(cd ${.CURDIR}; cat ${TESTS}) | grep DEFINE_TEST > list.h

clean:
	rm -f *.out
	rm -f *.o
	rm -f *.core
	rm -f *~
	rm -f list.h
	-chmod -R +w /tmp/libarchive_test.*
	rm -rf /tmp/libarchive_test.*

.include <bsd.prog.mk>
