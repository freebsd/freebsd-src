PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/cam/ctl

ATF_TESTS_SH+=	read_buffer

# Must be exclusive because it disables/enables camsim
TEST_METADATA.read_buffer+=	is_exclusive="true"

.include <bsd.test.mk>
