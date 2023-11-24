# $NetBSD: depsrc-usebefore-double-colon.mk,v 1.1 2020/08/22 08:29:13 rillig Exp $
#
# Tests for the special source .USEBEFORE in dependency declarations,
# combined with the double-colon dependency operator.

all: action

# The dependency operator :: allows commands to be added later to the same
# target.
double-colon:: .USEBEFORE
	@echo double-colon early 1

# This command is ignored, which kind of makes sense since this dependency
# declaration has no .USEBEFORE source.
double-colon::
	@echo double-colon early 2

# XXX: This command is ignored even though it has a .USEBEFORE source.
# This is unexpected.
double-colon:: .USEBEFORE
	@echo double-colon early 3

# At this point, the commands from the .USEBEFORE targets are copied to
# the "action" target.
action: double-colon

# This command is not added to the "action" target since it comes too late.
# The commands had been copied in the previous line already.
double-colon::
	@echo double-colon late
