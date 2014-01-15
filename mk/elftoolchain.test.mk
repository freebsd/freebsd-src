# $Id: elftoolchain.test.mk 2068 2011-10-26 15:49:07Z jkoshy $

#
# Rules for invoking test suites.
#

TEST_DIRECTORY=		test
TEST_TARGET=		test

# The special target 'test' runs the test suite associated with a
# utility or library.
test:	all .PHONY
	cd ${TOP}/${TEST_DIRECTORY}/${.CURDIR:T} && \
	${MAKE} all ${TEST_TARGET}
