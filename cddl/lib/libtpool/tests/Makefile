ATF_TESTS_C+=	libtpool_test

TEST_METADATA+= timeout="10"

CFLAGS+=	-I${ZFSTOP}/include \
		-I${ZFSTOP}/lib/libspl/include

LIBADD+=	pthread tpool

.include "${SRCTOP}/cddl/Makefile.inc"
.include <bsd.test.mk>
