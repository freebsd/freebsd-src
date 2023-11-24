# $NetBSD: job-output-null.mk,v 1.4 2022/09/03 08:03:27 rillig Exp $
#
# Test how null bytes in the output of a command are handled.  Make processes
# them using null-terminated strings, which may cut off some of the output.
#
# Before job.c 1.454 from 2022-09-03, make handled null bytes in the output
# from the child process inconsistently.  It's an edge case though since
# typically the child processes output text.

# Note: The printf commands used in this test must only use a single format
# string, without parameters.  This is because it is implementation-dependent
# how many times the command 'printf "fmt%s" "" "" ""' calls write(2).
#
#	NetBSD /bin/sh		1 x write("fmtfmtfmt")
#	Dash			1 x write("fmtfmtfmt")
#	NetBSD /bin/ksh		3 x write("fmt") (via /bin/printf)
#	Bash 5			3 x write("fmt")
#
# In the latter case the output may arrive in 1 to 3 parts, depending on the
# exact timing, which in this test makes a crucial difference since before
# job.c 1.454 from 2022-09-03, the outcome of the test depended on whether
# there was a '\n' in each of the blocks from the output.  Depending on the
# exact timing, the output of that test varied, its possible values were '2a',
# '2a 2b', '2a 2c', '2a 2b 2c'.

.MAKEFLAGS: -j1		# force jobs mode

all: .PHONY
	# The null byte from the command output is replaced with a single
	# space by CollectOutput.
	@printf '1\0trailing\n'
	# expect: 1 trailing

	# Give the parent process a chance to see the above output, but not
	# yet the output from the next printf command.
	@sleep 1

	# Each null byte from the command output is replaced with a single
	# space.
	@printf '2a\0trailing\n''2b\0trailing\n''2c\0trailing\n'
	# expect: 2a trailing
	# expect: 2b trailing
	# expect: 2c trailing

	@sleep 1

	# Each null byte from the command output is replaced with a single
	# space.  Because there is no trailing newline in the output, these
	# null bytes were replaced with spaces even before job.c 1.454 from
	# 2022-09-03, unlike in the cases above.
	#
	# The three null bytes in a row test whether this output is
	# compressed to a single space like in DebugFailedTarget.  It isn't.
	@printf '3a\0without\0\0\0newline, 3b\0without\0\0\0newline.'
	# expect: 3a without   newline, 3b without   newline.
