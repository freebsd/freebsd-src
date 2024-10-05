PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/cam/ctl

${PACKAGE}FILES+=	ctl.subr

ATF_TESTS_SH+=	prevent
ATF_TESTS_SH+=	read_buffer
ATF_TESTS_SH+=	start_stop_unit

# Must be exclusive because it disables/enables camsim
TEST_METADATA+=	is_exclusive="true"

.include <bsd.test.mk>
