# $NetBSD: dep-double-colon.mk,v 1.4 2020/09/26 15:41:53 rillig Exp $
#
# Tests for the :: operator in dependency declarations.

all::
	@echo 'command 1a'
	@echo 'command 1b'

all::
	@echo 'command 2a'
	@echo 'command 2b'

# When there are multiple command groups for a '::' target, each of these
# groups is added separately to the .ALLTARGETS variable.
#
# XXX: What is this good for?
# XXX: Where does the leading space come from?
.if ${.ALLTARGETS} != " all all"
.  error
.endif
