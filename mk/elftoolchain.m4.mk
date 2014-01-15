#
# $Id: elftoolchain.m4.mk 2795 2012-12-19 12:39:09Z jkoshy $
#

# Implicit rules for the M4 pre-processor.

.if !defined(TOP)
.error	Make variable \"TOP\" has not been defined.
.endif

.SUFFIXES:	.m4 .c
.m4.c:
	m4 -D SRCDIR=${.CURDIR} ${M4FLAGS} ${.IMPSRC} > ${.TARGET}

