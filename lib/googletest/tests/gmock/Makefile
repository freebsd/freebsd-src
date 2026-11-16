.include <bsd.init.mk>

.PATH:	${GOOGLEMOCK_SRCROOT}/src ${GOOGLEMOCK_SRCROOT}/test

# This doesn't work with the googletest engine in kyua 0.15.0.
PLAIN_TESTS_CXX+= gmock_stress_test

LIBADD+=	pthread gtest gmock

.include <bsd.test.mk>
