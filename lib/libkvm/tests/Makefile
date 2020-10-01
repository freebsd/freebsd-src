# $FreeBSD$

.include <bsd.own.mk>

ATF_TESTS_C+=	kvm_close_test
ATF_TESTS_C+=	kvm_geterr_test
ATF_TESTS_C+=	kvm_open_test
ATF_TESTS_C+=	kvm_open2_test
ATF_TESTS_C+=	kvm_read_test

CFLAGS.kvm_geterr_test+=	-I${.CURDIR:H}
CFLAGS.kvm_read_test+=		-I${.CURDIR:H}

LIBADD+=	kvm

BINDIR=		${TESTSDIR}

.for t in kvm_geterr_test kvm_open_test kvm_open2_test kvm_read_test
SRCS.$t=	$t.c kvm_test_common.c
.endfor

.include <bsd.test.mk>
