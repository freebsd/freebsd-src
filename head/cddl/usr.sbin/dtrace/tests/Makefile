# $FreeBSD$

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/cddl/usr.sbin/dtrace
TESTS_SUBDIRS+=	common

.PATH:		${.CURDIR:H:H:H:H}/tests
KYUAFILE=	YES

.PATH:		${.CURDIR:H:H:H}/contrib/opensolaris/cmd/dtrace/test/cmd/scripts
SCRIPTSDIR=	${TESTSDIR}
SCRIPTS=	dtest.pl

SUBDIR_PARALLEL=

.include <bsd.test.mk>
