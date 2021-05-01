# $NetBSD: depsrc-ignore.mk,v 1.5 2020/11/15 20:20:58 rillig Exp $
#
# Tests for the special source .IGNORE in dependency declarations,
# which ignores any command failures for that target.
#
# Even though 'ignore-errors' fails, 'all' is still made.  Since 'all' is
# not marked with .IGNORE, it stops at the first failing command.
#
# XXX: The ordering of the messages in the output is confusing.
# The "ignored" comes much too late to be related to the "false
# ignore-errors".  This is due to stdout being buffered.
#
# The "continuing" message comes from the -k option.  If there had been
# other targets independent of "all", these would be built as well.
#
# Enabling the debugging option -de changes the order in which the messages
# appear.  Now the "ignored" message is issued in the correct position.
# The explanation for the output reordering is that the output is buffered.
# As the manual page says, in debugging mode stdout is line buffered.
# In these tests the output is redirected to a file, therefore stdout is
# fully buffered.
#
# This is what actually happens, as of 2020-08-29.  To verify it, set the
# following breakpoints in CompatRunCommand:
#
# * the "!silent" line, to see all commands
# * the "fflush" line, to see stdout being flushed
# * the "status = WEXITSTATUS" line
# * the "(continuing)" line
# * the "(ignored)" line
#
# The breakpoints are visited in the following order:
#
# "ignore-errors begin"
#	Goes directly to STDOUT_FILENO since it is run in a child process.
# "false ignore-errors"
#	Goes to the stdout buffer (CompatRunCommand, keyword "!silent") and
#	the immediate call to fflush(stdout) copies it to STDOUT_FILENO.
# "*** Error code 1 (ignored)"
#	Goes to the stdout buffer but is not flushed (CompatRunCommand, near
#	the end).
# "ignore-errors end"
#	Goes directly to STDOUT_FILENO.
# "all begin"
#	Goes directly to STDOUT_FILENO.
# "false all"
#	Goes to the stdout buffer, where the "*** Error code 1 (ignored)" is
#	still waiting to be flushed.  These two lines are flushed now.
# "*** Error code 1 (continuing)"
#	Goes to the stdout buffer.
# "Stop."
#	Goes to the stdout buffer.
# exit(1)
#	Flushes the stdout buffer to STDOUT_FILENO.

all: ignore-errors

ignore-errors: .IGNORE
	@echo $@ begin
	false $@
	@echo $@ end

all:
	@echo $@ begin
	false $@
	@echo $@ end
