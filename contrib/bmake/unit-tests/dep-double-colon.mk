# $NetBSD: dep-double-colon.mk,v 1.3 2020/08/22 12:42:32 rillig Exp $
#
# Tests for the :: operator in dependency declarations.

all::
	@echo 'command 1a'
	@echo 'command 1b'

all::
	@echo 'command 2a'
	@echo 'command 2b'
