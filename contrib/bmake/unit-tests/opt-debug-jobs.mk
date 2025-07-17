# $NetBSD: opt-debug-jobs.mk,v 1.6 2023/11/19 21:47:52 rillig Exp $
#
# Tests for the -dj command line option, which adds debug logging about
# running jobs in multiple shells.

.MAKEFLAGS: -dj

# Run in parallel mode since the debug logging is more interesting there
# than in compat mode.
.MAKEFLAGS: -j1

all:
	# Only the actual command is logged.
	# To see the evaluation of the expressions, use -dv.
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

	# Avoid a race condition in the debug output.  Without sleeping,
	# it is not guaranteed that the two lines "exited/stopped" and
	# "JobFinish" are output earlier than the stdout of the actual shell
	# commands.  The '@' prefix avoids that this final command gets into
	# another race condition with the "exited/stopped" line.
	@sleep 1
