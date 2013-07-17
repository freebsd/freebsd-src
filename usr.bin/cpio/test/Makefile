# $FreeBSD$

LIBARCHIVEDIR=	${.CURDIR}/../../../contrib/libarchive

.PATH:	${LIBARCHIVEDIR}/cpio
CPIO_SRCS= cmdline.c

.PATH:	${LIBARCHIVEDIR}/libarchive_fe
CPIO_SRCS+= err.c

.PATH:	${LIBARCHIVEDIR}/cpio/test
TESTS=	\
	test_0.c				\
	test_basic.c				\
	test_cmdline.c				\
	test_extract_cpio_Z.c			\
	test_extract_cpio_bz2.c			\
	test_extract_cpio_grz.c			\
	test_extract_cpio_gz.c			\
	test_extract_cpio_lrz.c			\
	test_extract_cpio_lz.c			\
	test_extract_cpio_lzma.c		\
	test_extract_cpio_lzo.c			\
	test_extract_cpio_xz.c			\
	test_format_newc.c			\
	test_gcpio_compat.c			\
	test_option_0.c				\
	test_option_B_upper.c			\
	test_option_C_upper.c			\
	test_option_J_upper.c			\
	test_option_L_upper.c			\
	test_option_Z_upper.c			\
	test_option_a.c				\
	test_option_b64encode.c			\
	test_option_c.c				\
	test_option_d.c				\
	test_option_f.c				\
	test_option_grzip.c			\
	test_option_help.c			\
	test_option_l.c				\
	test_option_lrzip.c			\
	test_option_lzma.c			\
	test_option_lzop.c			\
	test_option_m.c				\
	test_option_t.c				\
	test_option_u.c				\
	test_option_uuencode.c			\
	test_option_version.c			\
	test_option_xz.c			\
	test_option_y.c				\
	test_option_z.c				\
	test_owner_parse.c			\
	test_passthrough_dotdot.c		\
	test_passthrough_reverse.c

# Build the test program
SRCS= list.h					\
	${CPIO_SRCS}				\
	${TESTS}				\
	main.c

.PATH:	${LIBARCHIVEDIR}/test_utils
SRCS+=	test_utils.c

CLEANFILES+= list.h bsdcpio_test

NO_MAN=yes

PROG=bsdcpio_test
DPADD=${LIBARCHIVE} ${LIBBZ2} ${LIBZ} ${LIBLZMA}
CFLAGS+=	-DPLATFORM_CONFIG_H=\"${.CURDIR}/../../../lib/libarchive/config_freebsd.h\"
CFLAGS+=	-I${.CURDIR}/../../../lib/libarchive -I${.OBJDIR}
LDADD= -larchive -lz -lbz2 -llzma
#CFLAGS+= -static -g -O2 -Wall
CFLAGS+= -g -O2 -Wall
CFLAGS+= -I${.OBJDIR}
CFLAGS+= -I${LIBARCHIVEDIR}/cpio -I${LIBARCHIVEDIR}/libarchive_fe
CFLAGS+= -I${LIBARCHIVEDIR}/test_utils

# Uncomment to link against dmalloc
#LDADD+= -L/usr/local/lib -ldmalloc
#CFLAGS+= -I/usr/local/include -DUSE_DMALLOC

check test:	bsdcpio_test
	${.OBJDIR}/bsdcpio_test -p ${.OBJDIR}/../bsdcpio \
	-r ${LIBARCHIVEDIR}/cpio/test

${.OBJDIR}/list.h list.h: ${TESTS} Makefile
	(cd ${LIBARCHIVEDIR}/cpio/test; cat ${TESTS}) | \
	grep DEFINE_TEST > ${.OBJDIR}/list.h

clean:
	rm -f ${CLEANFILES}
	rm -f *~
	-chmod -R +w /tmp/bsdcpio_test.*
	rm -rf /tmp/bsdcpio_test.*

.include <bsd.prog.mk>
