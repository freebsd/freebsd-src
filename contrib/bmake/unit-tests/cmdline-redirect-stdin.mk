# $NetBSD: cmdline-redirect-stdin.mk,v 1.1 2021/02/01 20:31:41 rillig Exp $
#
# Demonstrate that the '!=' assignment operator can read individual lines
# from make's stdin.
#
# This edge case is an implementation detail that has no practical
# application.

all: .PHONY
	@printf '%s\n' "first line" "second line" \
	| ${MAKE} -f ${MAKEFILE} read-lines

.if make(read-lines)
line1!=		read line; echo "$$line"
line2!=		read line; echo "$$line"

.if ${line1} != "first line"
.  error line1="${line1}"

.elif ${line2} == ""
# If this branch is ever reached, the shell from the assignment to line1
# probably buffers its input.  Most shells use unbuffered stdin, and this
# is actually specified by POSIX, which says that "The read utility shall
# read a single line from standard input".  This is the reason why the shell
# reads its input byte by byte, which makes it terribly slow for practical
# applications.
.  error The shell's read command does not read a single line.

.elif ${line2} != "second line"
.  error line2="${line2}"
.endif

read-lines: .PHONY
.endif
