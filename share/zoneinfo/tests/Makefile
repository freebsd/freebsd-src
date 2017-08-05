# $FreeBSD$

.include <bsd.own.mk>

.PATH: ${SRCTOP}/contrib/tzdata

FILESGROUPS=	FILES
FILESGROUPS+=	TESTFILES

.if ${MK_ZONEINFO_OLD_TIMEZONES_SUPPORT} != "no"
ATF_TESTS_SH+=	backward_test
TESTFILES+=	backward
.endif

TESTFILES+=	zoneinfo_common.sh
TESTFILESDIR=	${TESTSDIR}

.include <bsd.test.mk>
