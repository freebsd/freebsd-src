ATF_TESTS_SH+=	mixer_test

# mixer tests fiddle with a singleton dummy audio device.
TEST_METADATA.mixer_test=	is_exclusive=true \
				required_kmods="snd_dummy"

.include <bsd.test.mk>
