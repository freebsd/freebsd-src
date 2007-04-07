# $FreeBSD$

TESTS= \
	test_acl_basic.c			\
	test_acl_pax.c				\
	test_archive_api_feature.c		\
	test_bad_fd.c				\
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
	test_read_format_iso_gz.c		\
	test_read_format_isorr_bz2.c		\
	test_read_format_pax_bz2.c		\
	test_read_format_tar.c			\
	test_read_format_tbz.c			\
	test_read_format_tgz.c			\
	test_read_format_tz.c			\
	test_read_format_zip.c			\
	test_read_large.c			\
	test_read_position.c			\
	test_read_truncated.c			\
	test_write_disk.c			\
	test_write_disk_perms.c			\
	test_write_disk_secure.c		\
	test_write_format_ar.c			\
	test_write_format_cpio_empty.c		\
	test_write_format_shar_empty.c		\
	test_write_format_tar.c			\
	test_write_format_tar_empty.c		\
	test_write_open_memory.c

SRCS= ${TESTS}					\
	list.h					\
	main.c

CLEANFILES+= list.h

MK_MAN=no
NO_MAN=yes

PROG=libarchive_test
DPADD=${LIBARCHIVE} ${LIBBZ2} ${LIBZ}
LDADD= -larchive -lz -lbz2
CFLAGS+= -static -g
CFLAGS+= -I${.OBJDIR}

test:	libarchive_test
	./libarchive_test

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
