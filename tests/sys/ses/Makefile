PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/ses

ATF_TESTS_C+=	destructive
ATF_TESTS_C+=	nondestructive

# Some tests cases alter enclosure state, so they can't run concurrently.
TEST_METADATA.destructive+= is_exclusive=true

.include <bsd.test.mk>
