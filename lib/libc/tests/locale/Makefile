# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libc/locale

NETBSD_ATF_TESTS_C=	io_test
NETBSD_ATF_TESTS_C+=	mbrtowc_test
#NETBSD_ATF_TESTS_C+=	mbstowcs_test
NETBSD_ATF_TESTS_C+=	mbsnrtowcs_test
NETBSD_ATF_TESTS_C+=	mbtowc_test
NETBSD_ATF_TESTS_C+=	wcscspn_test
NETBSD_ATF_TESTS_C+=	wcspbrk_test
NETBSD_ATF_TESTS_C+=	wcsspn_test
NETBSD_ATF_TESTS_C+=	wcstod_test
NETBSD_ATF_TESTS_C+=	wctomb_test

CFLAGS.t_wctomb.c+= -Wno-stack-protector

.include "../Makefile.netbsd-tests"

.include <bsd.test.mk>
