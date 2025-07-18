.include <src.opts.mk>

ATF_TESTS_C+=		strptime_test
.if ${MK_DETECT_TZ_CHANGES} != "no"
ATF_TESTS_C+=		detect_tz_changes_test
.endif

TESTSDIR:=	${TESTSBASE}/${RELDIR:C/libc\/tests/libc/}

.include <bsd.test.mk>
