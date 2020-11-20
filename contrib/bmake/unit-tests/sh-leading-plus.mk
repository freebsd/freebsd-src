# $NetBSD: sh-leading-plus.mk,v 1.4 2020/11/09 20:50:56 rillig Exp $
#
# Tests for shell commands preceded by a '+', to run them even if
# the command line option -n is given.

.MAKEFLAGS: -n

all:
	@echo 'this command is not run'
	@+echo 'this command is run'
