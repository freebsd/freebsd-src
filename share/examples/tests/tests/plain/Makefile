# $FreeBSD$

.include <bsd.own.mk>

# Directory into which the Kyuafile provided by this directory will be
# installed.
#
# This is always a subdirectory of ${TESTSBASE}/.  The remainder of the
# path has to match the relative path within the source tree in which
# these files are found modulo the tests/ component at the end.
#
# For example: if this Makefile were in src/bin/cp/tests/, its TESTSDIR
# would point at ${TESTSBASE}/bin/cp/.
TESTSDIR=	${TESTSBASE}/share/examples/tests/plain

# List of test programs to build.  Note that we can build more than one
# test from a single directory, and this is expected.
PLAIN_TESTS_C=	printf_test
PLAIN_TESTS_SH=	cp_test

# Tell bsd.test.mk that we are providing a hand-crafted Kyuafile in this
# directory.  We do so because the file in this directory exists for
# documentation purposes.
#
# In general, however, you should NOT define KYUAFILE at all to allow
# bsd.test.mk auto-generate one for you based on the PLAIN_TESTS_*
# definitions from above.
KYUAFILE=	yes

.include <plain.test.mk>
