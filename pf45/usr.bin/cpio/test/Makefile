# $FreeBSD$

# Where to find the cpio sources (for the internal unit tests)
CPIO_SRCDIR=${.CURDIR}/..
.PATH: ${CPIO_SRCDIR}

# Some cpio sources are pulled in for white-box tests
CPIO_SRCS= cmdline.c err.c pathmatch.c

TESTS=	\
	test_0.c				\
	test_basic.c				\
	test_format_newc.c			\
	test_gcpio_compat.c			\
	test_option_a.c				\
	test_option_B.c				\
	test_option_c.c				\
	test_option_d.c				\
	test_option_f.c				\
	test_option_help.c			\
	test_option_L.c				\
	test_option_ell.c			\
	test_option_m.c				\
	test_option_t.c				\
	test_option_u.c				\
	test_option_version.c			\
	test_option_y.c				\
	test_option_z.c				\
	test_owner_parse.c			\
	test_passthrough_dotdot.c		\
	test_passthrough_reverse.c		\
	test_pathmatch.c

# Build the test program
SRCS= list.h					\
	${CPIO_SRCS}				\
	${TESTS}				\
	main.c

CLEANFILES+= list.h bsdcpio_test

NO_MAN=yes

PROG=bsdcpio_test
DPADD=${LIBARCHIVE} ${LIBBZ2} ${LIBZ} ${LIBLZMA}
CFLAGS+=	-DPLATFORM_CONFIG_H=\"config_freebsd.h\"
CFLAGS+=	-I..
LDADD= -larchive -lz -lbz2 -llzma
CFLAGS+= -static -g -O2 -Wall
CFLAGS+= -I${.OBJDIR}
CFLAGS+= -I${CPIO_SRCDIR}

# Uncomment to link against dmalloc
#LDADD+= -L/usr/local/lib -ldmalloc
#CFLAGS+= -I/usr/local/include -DUSE_DMALLOC

check test:	bsdcpio_test
	${.OBJDIR}/bsdcpio_test -p ${.OBJDIR}/../bsdcpio -r ${.CURDIR}

${.OBJDIR}/list.h list.h: ${TESTS} Makefile
	(cd ${.CURDIR}; cat ${TESTS}) | grep DEFINE_TEST > list.h

clean:
	rm -f ${CLEANFILES}
	rm -f *~
	-chmod -R +w /tmp/bsdcpio_test.*
	rm -rf /tmp/bsdcpio_test.*

.include <bsd.prog.mk>
