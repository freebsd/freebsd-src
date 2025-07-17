PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/cam/ctl
BINDIR=${TESTSDIR}

${PACKAGE}FILES+=	ctl.subr

ATF_TESTS_SH+=	persist
ATF_TESTS_SH+=	prevent
ATF_TESTS_SH+=	read_buffer
ATF_TESTS_SH+=	start_stop_unit

PROGS+=	prout_register_huge_cdb
LIBADD+=	cam
CFLAGS+=	-I${SRCTOP}/sys

# Must be exclusive because it disables/enables camsim
TEST_METADATA+=	is_exclusive="true"

.include <bsd.test.mk>
