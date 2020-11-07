# $NetBSD: sh-leading-plus.mk,v 1.3 2020/08/23 14:46:33 rillig Exp $
#
# Tests for shell commands preceded by a '+', to run them even if
# the command line option -n is given.

all:
	@echo 'this command is not run'
	@+echo 'this command is run'
