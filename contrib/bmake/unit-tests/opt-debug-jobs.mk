# $NetBSD: opt-debug-jobs.mk,v 1.4 2020/10/05 19:27:48 rillig Exp $
#
# Tests for the -dj command line option, which adds debug logging about
# running jobs in multiple shells.

.MAKEFLAGS: -dj

# Run in parallel mode since the debug logging is more interesting there
# than in compat mode.
.MAKEFLAGS: -j1

all:
	# Only the actual command is logged.
	# To see the evaluation of the variable expressions, use -dv.
	: ${:Uexpanded} expression

	# Undefined variables expand to empty strings.
	# Multiple spaces are preserved in the command, as they might be
	# significant.
	: ${UNDEF} variable

	# In the debug output, single quotes are not escaped, even though
	# the whole command is enclosed in single quotes as well.
	# This allows to copy and paste the whole command, without having
	# to unescape anything.
	: 'single' and "double" quotes
