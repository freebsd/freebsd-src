# $FreeBSD$
#
# Generic build infrastructure for test programs.
#
# This is the only public file that should be included by Makefiles when
# tests are to be built.  All other *.test.mk files are internal and not
# to be included directly.

.include <bsd.init.mk>

__<bsd.test.mk>__:

.ifndef TESTSDIR
.error "Please define TESTSDIR when including bsd.test.mk"
.endif

# List of subdirectories containing tests into which to recurse.  This has the
# same semantics as SUBDIR at build-time.  However, the directories listed here
# get registered into the run-time test suite definitions so that the test
# engines know to recurse into these directories.
#
# In other words: list here any directories that contain test programs but use
# SUBDIR for directories that may contain helper binaries and/or data files.
TESTS_SUBDIRS?=

# If defined, indicates that the tests built by the Makefile are not part of
# the FreeBSD Test Suite.  The implication of this is that the tests won't be
# installed under /usr/tests/ and that Kyua won't be able to run them.
#NOT_FOR_TEST_SUITE=

# List of variables to pass to the tests at run-time via the environment.
TESTS_ENV?=

# Force all tests in a separate distribution file.
#
# We want this to be the case even when the distribution name is already
# overriden.  For example: we want the tests for programs in the 'games'
# distribution to end up in the 'tests' distribution; the test programs
# themselves have all the necessary logic to detect that the games are not
# installed and thus won't cause false negatives.
DISTRIBUTION:=	tests

# Ordered list of directories to construct the PATH for the tests.
TESTS_PATH+= ${DESTDIR}/bin ${DESTDIR}/sbin \
             ${DESTDIR}/usr/bin ${DESTDIR}/usr/sbin
TESTS_ENV+= PATH=${TESTS_PATH:tW:C/ +/:/g}

# Ordered list of directories to construct the LD_LIBRARY_PATH for the tests.
TESTS_LD_LIBRARY_PATH+= ${DESTDIR}/lib ${DESTDIR}/usr/lib
TESTS_ENV+= LD_LIBRARY_PATH=${TESTS_LD_LIBRARY_PATH:tW:C/ +/:/g}

# List of all tests being built.  The various *.test.mk modules extend this
# variable as needed.
_TESTS=

# Pull in the definitions of all supported test interfaces.
.include <atf.test.mk>
.include <plain.test.mk>
.include <tap.test.mk>

.for ts in ${TESTS_SUBDIRS}
.if empty(SUBDIR:M${ts})
SUBDIR+= ${ts}
.endif
.endfor

# it is rare for test cases to have man pages
.if !defined(MAN)
MAN=
.endif

# tell progs.mk we might want to install things
PROG_VARS+= BINDIR
PROGS_TARGETS+= install

.if !defined(NOT_FOR_TEST_SUITE)
.include <suite.test.mk>
.endif

.if !target(realregress)
realregress: .PHONY
	@echo "$@ not defined; skipping"
.endif

beforeregress realregress afterregress regress: .PHONY
.ORDER: beforeregress realregress afterregress
regress: beforeregress realregress afterregress

.ifdef PROG
# we came here via bsd.progs.mk below
# parent will do staging.
MK_STAGING= no
.endif

.if !empty(PROGS) || !empty(PROGS_CXX) || !empty(SCRIPTS)
.include <bsd.progs.mk>
.endif
.include <bsd.files.mk>

.if !defined(PROG) && ${MK_STAGING} != "no"
.if !defined(_SKIP_BUILD)
# this will handle staging if needed
_SKIP_STAGING= no
# but we don't want it to build anything
_SKIP_BUILD=
.endif
.if !empty(PROGS)
stage_files.prog: ${PROGS}
.endif
.include <bsd.prog.mk>
.endif

.if !target(objwarn)
.include <bsd.obj.mk>
.endif
