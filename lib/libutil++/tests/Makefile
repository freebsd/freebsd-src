PACKAGE=	tests

ATF_TESTS_CXX+=	pidfile_test
ATF_TESTS_CXX+=	stringf_test
ATF_TESTS_CXX+=	up_test

CFLAGS+=	-I${SRCTOP}/lib/libutil++
LIBADD+=	util++

LIBADD.pidfile_test+= util

.include <bsd.test.mk>
