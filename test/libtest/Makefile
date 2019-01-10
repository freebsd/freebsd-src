# $Id$
#
# The test(3) API.

TOP=		../..

SUBDIR+=	bin
SUBDIR+=	lib
SUBDIR+=	driver
SUBDIR+=	examples

.if !make(install)
.include "$(TOP)/mk/elftoolchain.subdir.mk"
.else
install:	.SILENT .PHONY
	echo Nothing to install.
.endif
