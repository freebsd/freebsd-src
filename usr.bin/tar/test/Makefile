# $FreeBSD$

# Where to find the tar sources (for the internal unit tests)
TAR_SRCDIR=${.CURDIR}/..
.PATH: ${TAR_SRCDIR}

# Some tar sources are pulled in for white-box tests
TAR_SRCS=					\
	../getdate.c

TESTS=	\
	test_0.c				\
	test_basic.c				\
	test_copy.c				\
	test_getdate.c				\
	test_help.c				\
	test_option_T.c				\
	test_option_q.c				\
	test_option_s.c				\
	test_patterns.c				\
	test_stdio.c				\
	test_strip_components.c			\
	test_symlink_dir.c			\
	test_version.c

# Build the test program
SRCS= ${TAR_SRCS}				\
	${TESTS}				\
	list.h					\
	main.c

CLEANFILES+= list.h

NO_MAN=yes

PROG=bsdtar_test
DPADD=${LIBARCHIVE} ${LIBBZ2} ${LIBZ} ${LIBLZMA}
CFLAGS+=	-DPLATFORM_CONFIG_H=\"config_freebsd.h\"
CFLAGS+=	-I..
LDADD= -larchive -lz -lbz2 -llzma
CFLAGS+= -static -g -O2 -Wall
CFLAGS+= -I${.OBJDIR}
CFLAGS+= -I${TAR_SRCDIR}

# Uncomment to link against dmalloc
#LDADD+= -L/usr/local/lib -ldmalloc
#CFLAGS+= -I/usr/local/include -DUSE_DMALLOC

check test:	bsdtar_test
	./bsdtar_test -p ${.OBJDIR}/../bsdtar -r ${.CURDIR}

list.h: ${TESTS} Makefile
	(cd ${.CURDIR}; cat ${TESTS}) | grep DEFINE_TEST > list.h

clean:
	rm -f *.out
	rm -f *.o
	rm -f *.core
	rm -f *~
	rm -f list.h
	rm -f archive.h ../archive.h
	-chmod -R +w /tmp/bsdtar_test.*
	rm -rf /tmp/bsdtar_test.*

.include <bsd.prog.mk>
