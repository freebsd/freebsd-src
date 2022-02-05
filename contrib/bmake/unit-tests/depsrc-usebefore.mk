# $NetBSD: depsrc-usebefore.mk,v 1.7 2021/12/28 14:22:51 rillig Exp $
#
# Tests for the special source .USEBEFORE in dependency declarations,
# which allows to prepend common commands to other targets.
#
# See also:
#	.USE
#	depsrc-use.mk

# Before make.h 1.280 from 2021-12-28, a .USEBEFORE target was accidentally
# regarded as a candidate for the main target.  On the other hand, a .USE
# target was not.
not-a-main-candidate: .USEBEFORE

all: action directly

first: .USEBEFORE
	@echo first 1		# Using ${.TARGET} here would expand to "action"
	@echo first 2		# Using ${.TARGET} here would expand to "action"

second: .USEBEFORE
	@echo second 1
	@echo second 2

# It is possible but uncommon to have a .USEBEFORE target with no commands.
# This may happen as the result of expanding a .for loop.
empty: .USEBEFORE

# It is possible but uncommon to directly make a .USEBEFORE target.
directly: .USEBEFORE
	@echo directly

action: second first empty
