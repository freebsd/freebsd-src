.include <src.opts.mk>

ATF_TESTS_C+=		strptime_test
ATF_TESTS_C+=		detect_tz_changes_test

CFLAGS.detect_tz_changes_test+= -I${SRCTOP}/contrib/tzcode
.if ${MK_DETECT_TZ_CHANGES} != "no"
CFLAGS.detect_tz_changes_test+= -DDETECT_TZ_CHANGES
.endif

TESTSDIR:=	${TESTSBASE}/${RELDIR:C/libc\/tests/libc/}

.include <bsd.test.mk>
