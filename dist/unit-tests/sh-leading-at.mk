# $NetBSD: sh-leading-at.mk,v 1.3 2020/08/22 09:16:08 rillig Exp $
#
# Tests for shell commands preceded by an '@', to suppress printing
# the command to stdout.

all:
	@
	@echo 'ok'
	@ echo 'space after @'
	echo 'echoed'
