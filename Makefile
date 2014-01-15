# $Id: Makefile 2872 2013-01-07 13:57:54Z jkoshy $

TOP=	.

.include "${TOP}/mk/elftoolchain.os.mk"

# Build configuration information first.
SUBDIR += common

# Build the base libraries next.
SUBDIR += libelf
SUBDIR += libdwarf

# Build additional APIs.
SUBDIR += libelftc

# Build the tools needed for the rest of the build.
SUBDIR += isa

# Build tools after the libraries.
SUBDIR += addr2line
SUBDIR += ar
SUBDIR += brandelf
SUBDIR += cxxfilt
SUBDIR += elfcopy
SUBDIR += elfdump
SUBDIR += findtextrel
SUBDIR += nm
SUBDIR += readelf
SUBDIR += size
SUBDIR += strings
SUBDIR += tools

# Build the test suites.
.if exists(${.CURDIR}/test) && defined(MKTESTS) && ${MKTESTS} == "yes"
SUBDIR += test
.endif

# Build documentation at the end.
.if exists(${.CURDIR}/documentation) && defined(MKDOC) && ${MKDOC} == "yes"
SUBDIR += documentation
.endif

.include "${TOP}/mk/elftoolchain.subdir.mk"

#
# Special top-level targets.
#

# Run the test suites.
.if exists(${.CURDIR}/test) && defined(MKTESTS) && ${MKTESTS} == "yes"
run-tests:	all .PHONY
	(cd ${.CURDIR}/test; ${MAKE} test)
.endif
