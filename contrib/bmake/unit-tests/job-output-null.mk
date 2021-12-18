# $NetBSD: job-output-null.mk,v 1.3 2021/09/12 10:26:49 rillig Exp $
#
# Test how null bytes in the output of a command are handled.  Make processes
# them using null-terminated strings, which may cut off some of the output.
#
# As of 2021-04-15, make handles null bytes from the child process
# inconsistently.  It's an edge case though since typically the child
# processes output text.

# Note: The printf commands used in this test must only use a single format
# string, without parameters.  This is because it is implementation-dependent
# how many times the command 'printf "fmt%s" "" "" ""' calls write(2).
#
#	NetBSD /bin/sh		1 x write("fmtfmtfmt")
#	Dash			1 x write("fmtfmtfmt")
#	NetBSD /bin/ksh		3 x write("fmt") (via /bin/printf)
#	Bash 5			3 x write("fmt")
#
# In the latter case the output may arrive in parts, which in this test makes
# a crucial difference since the outcome of the test depends on whether there
# is a '\n' in each of the blocks from the output.

.MAKEFLAGS: -j1		# force jobs mode

all: .PHONY
	# The null byte from the command output is kept as-is.
	# See CollectOutput, which looks like it intended to replace these
	# null bytes with simple spaces.
	@printf '1\0trailing\n'

	# Give the parent process a chance to see the above output, but not
	# yet the output from the next printf command.
	@sleep 1

	# All null bytes from the command output are kept as-is.
	@printf '2a\0trailing\n''2b\0trailing\n''2c\0trailing\n'

	@sleep 1

	# The null bytes are replaced with spaces since they are not followed
	# by a newline.
	#
	# The three null bytes in a row test whether this output is
	# compressed to a single space like in DebugFailedTarget.  It isn't.
	@printf '3a\0without\0\0\0newline, 3b\0without\0\0\0newline.'
