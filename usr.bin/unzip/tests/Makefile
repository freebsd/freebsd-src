# $FreeBSD$

PACKAGE=	tests

_LIBARCHIVEDIR=	${SRCTOP}/contrib/libarchive

ATF_TESTS_SH+=	functional_test

BINDIR=		${TESTSDIR}

PROGS+=		bsdunzip_test

CFLAGS+=	-DPLATFORM_CONFIG_H=\"${SRCTOP}/lib/libarchive/config_freebsd.h\"
CFLAGS+=	-I${SRCTOP}/lib/libarchive -I${.OBJDIR}

CFLAGS+=	-I${.OBJDIR}
CFLAGS+=	-I${_LIBARCHIVEDIR}/unzip -I${_LIBARCHIVEDIR}/unzip/test
CFLAGS+=	-I${_LIBARCHIVEDIR}/libarchive
CFLAGS+=	-I${_LIBARCHIVEDIR}/libarchive_fe -I${_LIBARCHIVEDIR}/test_utils

# Uncomment to link against dmalloc
#LDADD+= -L/usr/local/lib -ldmalloc
#CFLAGS+= -I/usr/local/include -DUSE_DMALLOC

CFLAGS.test_utils.c+=	-Wno-cast-align

.PATH:	${_LIBARCHIVEDIR}/libarchive_fe
UNZIP_SRCS+= err.c

.PATH:	${_LIBARCHIVEDIR}/unzip/test
TESTS_SRCS=	\
	test_0.c 				\
	test_C.c				\
	test_L.c				\
	test_P_encryption.c			\
	test_Z1.c				\
	test_basic.c				\
	test_d.c				\
	test_glob.c				\
	test_j.c				\
	test_n.c				\
	test_not_exist.c			\
	test_o.c				\
	test_p.c				\
	test_q.c				\
	test_singlefile.c 			\
	test_t.c				\
	test_t_bad.c				\
	test_version.c				\
	test_x.c

SRCS.bsdunzip_test= list.h			\
	${UNZIP_SRCS}				\
	${TESTS_SRCS}

.PATH:	${_LIBARCHIVEDIR}/test_utils
SRCS.bsdunzip_test+=	test_main.c		\
			test_utils.c

LIBADD.bsdunzip_test=	archive

list.h: ${TESTS_SRCS} Makefile
	@(cd ${_LIBARCHIVEDIR}/unzip/test && \
	grep -h DEFINE_TEST ${.ALLSRC:N*Makefile}) > ${.TARGET}.tmp
	@mv ${.TARGET}.tmp ${.TARGET}

CLEANFILES+=	list.h list.h.tmp

${PACKAGE}FILES+= test_basic.zip.uu
${PACKAGE}FILES+= test_encrypted.zip.uu
${PACKAGE}FILES+= test_singlefile.zip.uu
${PACKAGE}FILES+= test_t_bad.zip.uu

.include <bsd.test.mk>
