.include <src.opts.mk>

PACKAGE= tests

TESTSDIR= ${TESTSBASE}

${PACKAGE}FILES+=		README __init__.py conftest.py

KYUAFILE= yes

SUBDIR+= etc
SUBDIR+= examples
SUBDIR+= include
SUBDIR+= sys
SUBDIR+= atf_python
.if ${MK_CDDL} != "no"
SUBDIR+= oclo
.endif

SUBDIR_PARALLEL=

afterinstall: install-tests-local
install-tests-local: .PHONY
	${INSTALL_SYMLINK} -T 'package=tests' \
	    ../local/tests ${DESTDIR}${TESTSDIR}/local

.include "Makefile.inc0"
.include <bsd.test.mk>
