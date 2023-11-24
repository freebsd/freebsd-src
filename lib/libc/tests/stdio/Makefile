
.include <bsd.own.mk>

ATF_TESTS_C+=		eintr_test
ATF_TESTS_C+=		fdopen_test
ATF_TESTS_C+=		flushlbuf_test
ATF_TESTS_C+=		fmemopen2_test
ATF_TESTS_C+=		fopen2_test
ATF_TESTS_C+=		freopen_test
ATF_TESTS_C+=		getdelim_test
ATF_TESTS_C+=		gets_s_test
ATF_TESTS_C+=		mkostemp_test
ATF_TESTS_C+=		open_memstream2_test
ATF_TESTS_C+=		open_wmemstream_test
ATF_TESTS_C+=		perror_test
ATF_TESTS_C+=		print_positional_test
ATF_TESTS_C+=		printbasic_test
ATF_TESTS_C+=		printfloat_test
ATF_TESTS_C+=		scanfloat_test
ATF_TESTS_C+=		snprintf_test
ATF_TESTS_C+=		sscanf_test
ATF_TESTS_C+=		swprintf_test
ATF_TESTS_C+=		swscanf_test

SRCS.fopen2_test=	fopen_test.c

NETBSD_ATF_TESTS_C=	clearerr_test
NETBSD_ATF_TESTS_C+=	fflush_test
NETBSD_ATF_TESTS_C+=	fmemopen_test
NETBSD_ATF_TESTS_C+=	fopen_test
NETBSD_ATF_TESTS_C+=	fputc_test
NETBSD_ATF_TESTS_C+=	mktemp_test
NETBSD_ATF_TESTS_C+=	open_memstream_test
NETBSD_ATF_TESTS_C+=	popen_test
NETBSD_ATF_TESTS_C+=	printf_test
NETBSD_ATF_TESTS_C+=	scanf_test

LIBADD.eintr_test+=	 md
LIBADD.printfloat_test+= m
LIBADD.scanfloat_test+=  m

# Older toolchains won't understand C23 %b, %wN, %wfN
PROG_OVERRIDE_VARS+=	NO_WFORMAT
NO_WFORMAT.snprintf_test=
NO_WFORMAT.sscanf_test=
NO_WFORMAT.swprintf_test=
NO_WFORMAT.swscanf_test=

.if ${COMPILER_TYPE} == "gcc"
# 90: use of assignment suppression and length modifier together in scanf format
NO_WFORMAT.scanfloat_test=
.endif

.include "../Makefile.netbsd-tests"

.include <bsd.test.mk>
