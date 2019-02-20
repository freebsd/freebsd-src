# $FreeBSD$

.include <bsd.init.mk>

.PATH: ${GOOGLEMOCK_SRCROOT}/src ${GOOGLEMOCK_SRCROOT}/test

GTESTS+=	gmock_stress_test

LIBADD+=	pthread gtest gmock

# XXX: https://github.com/google/googletest/pull/2119
NO_WERROR=

.include <bsd.test.mk>
