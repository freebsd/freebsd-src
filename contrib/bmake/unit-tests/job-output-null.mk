# $NetBSD: job-output-null.mk,v 1.1 2021/04/15 19:02:29 rillig Exp $
#
# Test how null bytes in the output of a command are handled.  Make processes
# them using null-terminated strings, which may cut off some of the output.
#
# As of 2021-04-15, make handles null bytes from the child process
# inconsistently.  It's an edge case though since typically the child
# processes output text.

.MAKEFLAGS: -j1		# force jobs mode

all: .PHONY
	# The null byte from the command output is kept as-is.
	# See CollectOutput, which looks like it intended to replace these
	# null bytes with simple spaces.
	@printf 'hello\0world%s\n' ''

	# Give the parent process a chance to see the above output, but not
	# yet the output from the next printf command.
	@sleep 1

	# All null bytes from the command output are kept as-is.
	@printf 'hello\0world%s\n' '' '' '' '' '' ''

	@sleep 1

	# The null bytes are replaced with spaces since they are not followed
	# by a newline.
	#
	# The three null bytes in a row test whether this output is
	# compressed to a single space like in DebugFailedTarget.  It isn't.
	@printf 'hello\0world\0without\0\0\0newline%s' ', ' ', ' '.'
