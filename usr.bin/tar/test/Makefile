# $FreeBSD$

LIBARCHIVEDIR=	${.CURDIR}/../../../contrib/libarchive

.PATH: ${LIBARCHIVEDIR}/tar
# Some tar sources are pulled in for white-box tests
TAR_SRCS=						\
	getdate.c

.PATH: ${LIBARCHIVEDIR}/tar/test
TESTS=	\
	test_0.c				\
	test_basic.c				\
	test_copy.c				\
	test_empty_mtree.c			\
	test_getdate.c				\
	test_help.c				\
	test_option_T_upper.c			\
	test_option_q.c				\
	test_option_r.c				\
	test_option_s.c				\
	test_patterns.c				\
	test_stdio.c				\
	test_strip_components.c			\
	test_symlink_dir.c			\
	test_version.c

# Build the test program
SRCS=	${TAR_SRCS}				\
	${TESTS}				\
	list.h					\
	main.c

CLEANFILES+= list.h

NO_MAN=yes

PROG=bsdtar_test
DPADD=${LIBARCHIVE} ${LIBBZ2} ${LIBZ} ${LIBLZMA}
CFLAGS+=	-DPLATFORM_CONFIG_H=\"${.CURDIR}/../config_freebsd.h\"
LDADD= -larchive -lz -lbz2 -llzma
CFLAGS+= -static -g -O2 -Wall
CFLAGS+= -I${.CURDIR}/.. -I${.OBJDIR}
CFLAGS+= -I${LIBARCHIVEDIR}/tar

# Uncomment to link against dmalloc
#LDADD+= -L/usr/local/lib -ldmalloc
#CFLAGS+= -I/usr/local/include -DUSE_DMALLOC

check test:	bsdtar_test
	./bsdtar_test -p ${.OBJDIR}/../bsdtar -r ${LIBARCHIVEDIR}/tar/test

list.h: ${TESTS} Makefile
	(cd ${LIBARCHIVEDIR}/tar/test; cat ${TESTS}) | \
	grep DEFINE_TEST > ${.OBJDIR}/list.h

clean:
	rm -f ${CLEANFILES}
	rm -f *.out
	rm -f *.o
	rm -f *.core
	rm -f *~
	rm -f list.h
	rm -f archive.h ../archive.h
	-chmod -R +w /tmp/bsdtar_test.*
	rm -rf /tmp/bsdtar_test.*

.include <bsd.prog.mk>
