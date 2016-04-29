# $FreeBSD$

.include <bsd.own.mk>

FILESGROUPS=	FILES
PACKAGE=	tests
FILESGROUPS+=	TESTS
TESTSPACKAGE=	${PACKAGE}
TESTSDIR=	${TESTSBASE}/lib/atf
TESTS+=		Kyuafile


.PATH:		${SRCTOP}/tests
KYUAFILE=	yes

SUBDIR=		test-programs

.include <bsd.test.mk>
