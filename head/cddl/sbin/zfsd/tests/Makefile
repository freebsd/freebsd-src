# $FreeBSD$

SRCDIR=${.CURDIR}/../../../..
.include "${.CURDIR}/../Makefile.common"
.PATH:	${.CURDIR}/..

PROG_CXX=	zfsd_unittest
SRCS:=		${SRCS:Nzfsd_main.cc}
SRCS+=		libmocks.c zfsd_unittest.cc
CLEANFILES+=	*.gcno *.gcda *.info
CLEANDIRS+=	lcov-report

# Use #include <zfsd/xxx.h> in test programs.
INCFLAGS+=	-I ${.CURDIR}/../..

.if defined(DESTDIR)
INCFLAGS+=	-I ${DESTDIR}/usr/include
LIBRARY_PATH=	${DESTDIR}/lib:${DESTDIR}/usr/lib
LDFLAGS+=	-L ${DESTDIR}/lib -L ${DESTDIR}/usr/lib
.elif defined(WORLDTMP)
INCFLAGS+=	-I ${WORLDTMP}/usr/include
LIBRARY_PATH=	${WORLDTMP}/lib:${WORLDTMP}/usr/lib
LDFLAGS+=	-L ${WORLDTMP}/lib -L ${WORLDTMP}/usr/lib
.else
LIBRARY_PATH=
.endif
ZFSD_UNITTEST=	env LD_LIBRARY_PATH=${LIBRARY_PATH} ./zfsd_unittest

# Extra tools
LCOV=		lcov

# Googletest options
LOCALBASE?=	/usr/local
INCFLAGS+=	-I ${LOCALBASE}/include -D_THREAD_SAFE -pthread
LDFLAGS+=	-L ${LOCALBASE}/lib -D_THREAD_SAFE -pthread
LDADD+=		${LOCALBASE}/lib/libgtest.a

# GoogleMock options
LDADD+= ${LOCALBASE}/lib/libgmock.a ${LOCALBASE}/lib/libgmock_main.a

# Googlemock fails if we don't have this line
# https://groups.google.com/forum/#!msg/googletestframework/h8ixEPCFm0o/amwfu4xGJb0J
CFLAGS+= -DGTEST_HAS_PTHREAD

# GCOV options
CFLAGS+= -fprofile-arcs -ftest-coverage
LDADD+= -lgcov

all: tests

# Install the tests
TESTSBASE?=	/usr/tests
TESTSDIR=	${TESTSBASE}/zfsd
# TODO: Convert from an ATF SH test to a Kyua plain test
# Long term TODO: Convert to a Kyua googletest test
TESTS_SH+=	zfsd_test
BINDIR=		${TESTSDIR}

# Install the gcov annotation files too
FILESDIR=	${TESTSDIR}
GCNOS=		${SRCS:C/.c+$/.gcno/}
${GCNOS}:	${SRCS:C/.c+$/.o/} 
FILES=		${GCNOS}


# Run the tests and produce the coverage report
EXCLUDE_PATTERNS='/usr/include/*' '${LOCALBASE}/include/*'
EXCLUDE_PATTERNS+= '*/cddl/compat/opensolaris/include/*'
EXCLUDE_PATTERNS+= '*/tools/regression/zfsd/*'
.PHONY: tests
tests: zfsd_unittest
	${ZFSD_UNITTEST} --gmock_verbose=error

.PHONY: lcov
lcov: zfsd_unittest
	${LCOV} -z -d . -f && \
	${ZFSD_UNITTEST} --gmock_verbose=error
	${LCOV} -f -d . -c -o default.info && \
	${LCOV} -r default.info $(EXCLUDE_PATTERNS) -o trimmed.info && \
	mkdir -p lcov-report && \
	genhtml -o lcov-report trimmed.info

.include <atf.test.mk>
