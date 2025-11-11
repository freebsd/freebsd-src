PACKAGE=	tests

TESTSDIR=	${TESTSBASE}/sys/sound

ATF_TESTS_C+=	pcm_read_write
ATF_TESTS_C+=	polling
ATF_TESTS_C+=	sndstat

CFLAGS+=	-I${SRCTOP}/sys
LDFLAGS+=	-lnv

CFLAGS.pcm_read_write.c+=	-Wno-cast-align

.include <bsd.test.mk>
