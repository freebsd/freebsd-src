# $FreeBSD$

SRCTOP=		${.CURDIR:H:H:H}
TESTSDIR=	${TESTSBASE}/usr.bin/cpio

LIBARCHIVEDIR=	${SRCTOP}/contrib/libarchive

ATF_TESTS_SH+=	functional_test

BINDIR=		${TESTSDIR}

PROGS+=		bsdcpio_test

CFLAGS+=	-DPLATFORM_CONFIG_H=\"${SRCTOP}/lib/libarchive/config_freebsd.h\"
CFLAGS+=	-I${SRCTOP}/lib/libarchive -I${.OBJDIR}

CFLAGS+=	-I${.OBJDIR}
CFLAGS+=	-I${LIBARCHIVEDIR}/cpio -I${LIBARCHIVEDIR}/libarchive_fe
CFLAGS+=	-I${LIBARCHIVEDIR}/test_utils

# Uncomment to link against dmalloc
#LDADD+= -L/usr/local/lib -ldmalloc
#CFLAGS+= -I/usr/local/include -DUSE_DMALLOC

.PATH:	${LIBARCHIVEDIR}/cpio
CPIO_SRCS= cmdline.c

.PATH:	${LIBARCHIVEDIR}/libarchive_fe
CPIO_SRCS+= err.c

.PATH:	${LIBARCHIVEDIR}/cpio/test
TESTS_SRCS=	\
	test_0.c				\
	test_basic.c				\
	test_cmdline.c				\
	test_extract_cpio_Z.c			\
	test_extract_cpio_bz2.c			\
	test_extract_cpio_grz.c			\
	test_extract_cpio_gz.c			\
	test_extract_cpio_lrz.c			\
	test_extract_cpio_lz.c			\
	test_extract_cpio_lz4.c			\
	test_extract_cpio_lzma.c		\
	test_extract_cpio_lzo.c			\
	test_extract_cpio_xz.c			\
	test_format_newc.c			\
	test_gcpio_compat.c			\
	test_missing_file.c			\
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
	test_option_lz4.c			\
	test_option_lzma.c			\
	test_option_lzop.c			\
	test_option_m.c				\
	test_option_passphrase.c		\
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

SRCS.bsdcpio_test= list.h			\
	${CPIO_SRCS}				\
	${TESTS_SRCS}				\
	main.c

.PATH:	${LIBARCHIVEDIR}/test_utils
SRCS.bsdcpio_test+=	test_utils.c

DPADD.bsdcpio_test=	${LIBARCHIVE}
LDADD.bsdcpio_test=	-larchive

list.h: ${TESTS_SRCS} Makefile
	@(cd ${LIBARCHIVEDIR}/tar/test && \
	grep -h DEFINE_TEST ${.ALLSRC:N*Makefile}) > ${.TARGET}.tmp
	@mv ${.TARGET}.tmp ${.TARGET}

CLEANFILES+=	list.h list.h.tmp

FILES+= test_extract.cpio.Z.uu
FILES+= test_extract.cpio.bz2.uu
FILES+= test_extract.cpio.grz.uu
FILES+= test_extract.cpio.gz.uu
FILES+= test_extract.cpio.lrz.uu
FILES+= test_extract.cpio.lz.uu
FILES+=	test_extract.cpio.lz4.uu
FILES+= test_extract.cpio.lzma.uu
FILES+= test_extract.cpio.lzo.uu
FILES+= test_extract.cpio.xz.uu
FILES+= test_gcpio_compat_ref.bin.uu
FILES+= test_gcpio_compat_ref.crc.uu
FILES+= test_gcpio_compat_ref.newc.uu
FILES+= test_gcpio_compat_ref.ustar.uu
FILES+= test_gcpio_compat_ref_nosym.bin.uu
FILES+= test_gcpio_compat_ref_nosym.crc.uu
FILES+= test_gcpio_compat_ref_nosym.newc.uu
FILES+= test_gcpio_compat_ref_nosym.ustar.uu
FILES+= test_option_f.cpio.uu
FILES+= test_option_m.cpio.uu
FILES+=	test_option_passphrase.zip.uu
FILES+= test_option_t.cpio.uu
FILES+= test_option_t.stdout.uu
FILES+= test_option_tv.stdout.uu

.include <bsd.test.mk>
