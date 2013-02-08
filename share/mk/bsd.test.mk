# $NetBSD: bsd.test.mk,v 1.21 2012/08/25 22:21:16 jmmv Exp $
# $FreeBSD$

.include <bsd.init.mk>

.if defined(TESTS_C)
PROGS+=	${TESTS_C}
.for _T in ${TESTS_C}
BINDIR.${_T}= ${TESTSDIR}
MAN.${_T}?= # empty
.endfor
.endif

.if defined(TESTS_CXX)
PROGS_CXX+= ${TESTS_CXX}
PROGS+= ${TESTS_CXX}
.for _T in ${TESTS_CXX}
BINDIR.${_T}= ${TESTSDIR}
MAN.${_T}?= # empty
.endfor
.endif

.if defined(TESTS_SH)
SCRIPTS+= ${TESTS_SH}
.for _T in ${TESTS_SH}
SCRIPTSDIR_${_T}= ${TESTSDIR}
.endfor
.endif

TESTSBASE?= ${DESTDIR}/usr/tests

# it is rare for test cases to have man pages
.if !defined(MAN)
WITHOUT_MAN=yes
.export WITHOUT_MAN
.endif

# tell progs.mk we might want to install things
PROG_VARS+= BINDIR
PROGS_TARGETS+= install

.if !empty(PROGS) || !empty(PROGS_CXX) || !empty(SCRIPTS)
.include <bsd.progs.mk>
.endif

beforetest: .PHONY
.if defined(TESTSDIR)
.if ${TESTSDIR} == ${TESTSBASE}
# Forbid running from ${TESTSBASE}.  It can cause false positives/negatives and
# it does not cover all the tests (e.g. it misses testing software in external).
	@echo "*** Sorry, you cannot use make test from src/tests.  Install the"
	@echo "*** tests into their final location and run them from ${TESTSBASE}"
	@false
.else
	@echo "*** Using this test does not preclude you from running the tests"
	@echo "*** installed in ${TESTSBASE}.  This test run may raise false"
	@echo "*** positives and/or false negatives."
.endif
.else
	@echo "*** No TESTSDIR defined; nothing to do."
	@false
.endif
	@echo

.if !target(realtest)
realtest: .PHONY
	@echo "$@ not defined; skipping"
.endif

test: .PHONY
.ORDER: beforetest realtest
test: beforetest realtest

.if target(aftertest)
.ORDER: realtest aftertest
test: aftertest
.endif

.include <bsd.obj.mk>
