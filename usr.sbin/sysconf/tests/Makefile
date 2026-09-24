ATF_TESTS_SH+=	sysconf_test

.include <bsd.test.mk>

#
# SCRIPTS install before DIRS in bsd.prog.mk; create TESTSDIR first so a
# leaf `make install' works before installworld has applied BSD.tests.dist.
#
beforeinstall: _sysconf_testdir
_sysconf_testdir: .PHONY
	${INSTALL} -d -o ${BINOWN} -g ${BINGRP} -m 0755 ${DESTDIR}${TESTSDIR}
