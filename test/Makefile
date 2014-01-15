# $Id: Makefile 2827 2012-12-29 16:43:15Z jkoshy $
#
# TET-based test suites used by the Elftoolchain project.
#

TOP=		..

# Build TET first.
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
