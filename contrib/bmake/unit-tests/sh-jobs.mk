# $NetBSD: sh-jobs.mk,v 1.3 2020/12/11 01:06:10 rillig Exp $
#
# Tests for the "run in jobs mode" part of the "Shell Commands" section
# from the manual page.

# TODO: Tutorial

.MAKEFLAGS: -j1

all: .PHONY comment .WAIT comment-with-followup-line .WAIT no-comment

# If a shell command starts with a comment character after stripping the
# leading '@', it is run in ignore-errors mode since the default runChkTmpl
# would lead to a syntax error in the generated shell file, at least for
# bash and dash, but not for NetBSD sh and ksh.
#
# See JobPrintCommand, cmdTemplate, runIgnTmpl
comment: .PHONY
	@# comment

# If a shell command starts with a comment character after stripping the
# leading '@', it is run in ignore-errors mode.
#
# See JobPrintCommand, cmdTemplate, runIgnTmpl
comment-with-followup-line: .PHONY
	@# comment${.newline}echo '$@: This is printed.'; false
	@true

# Without the comment, the commands are run in the default mode, which checks
# the exit status of every makefile line.
#
# See JobPrintCommand, cmdTemplate, runChkTmpl
no-comment: .PHONY
	@echo '$@: This is printed.'; false
	@true
