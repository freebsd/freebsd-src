# $FreeBSD$

.include <bsd.own.mk>

SUBDIR+= etc
SUBDIR+= sys
SUBDIR_PARALLEL=

TESTSDIR= ${TESTSBASE}
KYUAFILE= yes

afterinstall: install-tests-local
install-tests-local: .PHONY
	${INSTALL_SYMLINK} ../local/tests ${DESTDIR}${TESTSDIR}/local

.include <bsd.test.mk>
