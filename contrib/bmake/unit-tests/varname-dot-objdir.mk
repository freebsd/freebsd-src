# $NetBSD: varname-dot-objdir.mk,v 1.3 2024/06/01 11:06:17 rillig Exp $
#
# Tests for the special .OBJDIR variable.

# TODO: Implementation

all:
	# Add an entry to the cached_realpath table, to test cleaning up
	# that table in purge_relative_cached_realpaths.
	# Having a ':=' assignment in the command line is construed but works
	# well enough to reach the code.
	@${MAKE} -f ${MAKEFILE} 'VAR:=$${:U.:tA}' purge-cache

purge-cache:
	: ${.TARGET} was reached.
