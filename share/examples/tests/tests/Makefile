# $FreeBSD$

.include <bsd.own.mk>

PACKAGE=	tests
FILESGROUPS=	TESTS
TESTSPACKAGE=	${PACKAGE}

# Directory into which the Kyuafile provided by this directory will be
# installed.
#
# This is always a subdirectory of ${TESTSBASE}/.  The remainder of the
# path has to match the relative path within the source tree in which
# these files are found modulo the tests/ component at the end.
#
# For example: if this Makefile were in src/bin/cp/tests/, its TESTSDIR
# would point at ${TESTSBASE}/bin/cp/.
TESTSDIR=	${TESTSBASE}/share/examples/tests

# List of subdirectories into which we want to recurse during the build
# of the system.  We use TESTS_SUBDIRS instead of SUBDIR because we want
# the auto-generated Kyuafile to recurse into these directories.
TESTS_SUBDIRS=	atf plain

# We leave KYUAFILE unset so that bsd.test.mk auto-generates a Kyuafile
# for us based on the contents of the TESTS_SUBDIRS line above.  The
# generated file will tell the tests run-time engine to recurse into the
# directories listed above.
#KYUAFILE=	auto

.include <bsd.test.mk>
