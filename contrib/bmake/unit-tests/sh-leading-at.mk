# $NetBSD: sh-leading-at.mk,v 1.6 2023/01/19 19:55:27 rillig Exp $
#
# Tests for shell commands preceded by an '@', to suppress printing
# the command to stdout.
#
# See also:
#	.SILENT
#	depsrc-silent.mk
#	opt-silent.mk

all:
	@
	@echo 'ok'
	@ echo 'space after @'
	echo 'echoed'
	# The leading '@' can be repeated.
	@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
	@@@echo '3'

	# Since 2023-01-17, the leading '@', '+' and '-' may contain
	# whitespace, for compatibility with GNU make.
	@ @ @ echo 'whitespace in leading part'
