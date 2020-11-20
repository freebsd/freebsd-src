# $NetBSD: sh-leading-at.mk,v 1.5 2020/11/15 20:20:58 rillig Exp $
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
