# $FreeBSD$

.include <bsd.own.mk>

FILESGROUPS=	FILES
PACKAGE=	tests
FILESGROUPS+=	TESTS
TESTSPACKAGE=	${PACKAGE}
TESTS+=		Kyuafile

.PATH:		${SRCTOP}/tests
KYUAFILE=	yes

FILESDIR=	${TESTSDIR}
FILES=		regress.m4

.include <bsd.test.mk>
