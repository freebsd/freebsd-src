# $FreeBSD$

.include <bsd.own.mk>

PACKAGE=	tests
FILESGROUPS=	TESTS
TESTSPACKAGE=	${PACKAGE}
SUBDIR+=	libpythagoras target

SUBDIR_DEPEND_target=	libpythagoras
ATF_TESTS_C=	ld_library_pathfds

.include <bsd.test.mk>
