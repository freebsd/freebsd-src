# $FreeBSD$

.include <bsd.own.mk>

PACKAGE= tests
FILESGROUPS=	TESTS
TESTSPACKAGE=	${PACKAGE}
TESTS+=		Kyuafile

SUBDIR+= etc
SUBDIR+= sys
SUBDIR_PARALLEL=

TESTSDIR= ${TESTSBASE}
KYUAFILE= yes

FILES+= README
FILESDIR_README= ${TESTSDIR}

afterinstall: install-tests-local
install-tests-local: .PHONY
	${INSTALL_SYMLINK} ../local/tests ${DESTDIR}${TESTSDIR}/local

.include <bsd.test.mk>
