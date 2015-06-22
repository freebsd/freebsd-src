# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/cddl/usr.sbin/dtrace
TESTS_SUBDIRS+=	common

.PATH:		${.CURDIR:H:H:H:H}/tests
KYUAFILE=	YES

.PATH:		${.CURDIR}/tools
SCRIPTSDIR=	${TESTSDIR}
SCRIPTS=	dtest.sh

.include <bsd.test.mk>
