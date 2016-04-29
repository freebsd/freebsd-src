# $FreeBSD$

.include <bsd.own.mk>

FILESGROUPS=	FILES
PACKAGE=	tests
FILESGROUPS+=	TESTS
TESTSPACKAGE=	${PACKAGE}
TESTS+=		Kyuafile

TESTSDIR=	${TESTSBASE}/etc

.PATH:		${SRCTOP}/tests
KYUAFILE=	yes

SUBDIR+=	rc.d

.include <bsd.test.mk>
