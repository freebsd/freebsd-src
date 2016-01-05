# $FreeBSD$

TESTSDIR=	${TESTSBASE}/lib/libutil

TAP_TESTS_C+=	flopen_test
TAP_TESTS_C+=	grp_test
TAP_TESTS_C+=	humanize_number_test
.if ${MACHINE_CPUARCH} != "aarch64" # PR202304: pidfile_test hangs on arm64
TAP_TESTS_C+=	pidfile_test
.endif
TAP_TESTS_C+=	trimdomain_test
TAP_TESTS_C+=	trimdomain-nodomain_test

LIBADD+=	util

.include <bsd.test.mk>
