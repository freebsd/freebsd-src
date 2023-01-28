# $NetBSD: sh-leading-hyphen.mk,v 1.4 2023/01/19 19:55:27 rillig Exp $
#
# Tests for shell commands preceded by a '-', to ignore the exit status of
# the command line.
#
# See also:
#	.IGNORE
#	depsrc-ignore.mk
#	opt-ignore.mk

# TODO: Implementation

all:
	-true
	-false

	# An undefined variable expands to an empty string, without warning.
	# This is used in practice for prefixing tool names or for DESTDIR.
	# The '-' before 'unknown' is interpreted by make as '.IGNORE' flag.
	${UNDEF}-unknown-command 'needed for needshell in compat.c'

	# Expanding undefined variables may lead to strange error messages
	# when the shell interprets single-character options as commands
	# instead.
	${UNDEF} --unknown-long-option 'needed for needshell in compat.c'

	# Since 2023-01-17, the leading '@', '+' and '-' may contain
	# whitespace, for compatibility with GNU make.
	- - - @echo 'whitespace in leading part'
