PACKAGE=	tests

ATF_TESTS_C=	pfctl_test
ATF_TESTS_SH=	macro

LIBADD+=	sbuf
SUBDIR+=	files
WARNS=6

pfctl_test.o: pfctl_test_list.inc

.include <bsd.test.mk>
