# $NetBSD: deptgt-error.mk,v 1.4 2022/01/22 21:50:41 rillig Exp $
#
# Tests for the special target .ERROR in dependency declarations, which
# is made when another target fails.

all: .PHONY
	false fails

.ERROR:
	@echo 'Making ${.TARGET} out of nothing.'

.ERROR: sub-error
sub-error: .PHONY
	@echo 'Making ${.TARGET} as prerequisite.'

# Before making the '.ERROR' target, these variable values are printed.
MAKE_PRINT_VAR_ON_ERROR=	ERROR_INFO

# Use single quotes to demonstrate that the output is only informational, it
# does not use any established escaping mechanism.
ERROR_INFO=	This information is ${:Uprinted} on 'errors'.
