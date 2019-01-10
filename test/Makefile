# $Id: Makefile 3653 2018-12-25 12:31:28Z jkoshy $
#
# TET-based test suites used by the Elftoolchain project.
#

TOP=		..

# Build TET and LIBTEST first.
SUBDIR+=	libtest
SUBDIR+=	tet

# Build tests for libraries.
SUBDIR+=	libelf
SUBDIR+=	libelftc
SUBDIR+=	libdwarf

# Build tests for tools.
SUBDIR+=	ar
SUBDIR+=	elfcopy
SUBDIR+=	elfdump
SUBDIR+=	nm

.if !make(install)
.include "$(TOP)/mk/elftoolchain.subdir.mk"
.else
install:	.SILENT .PHONY
	echo Nothing to install.
.endif
