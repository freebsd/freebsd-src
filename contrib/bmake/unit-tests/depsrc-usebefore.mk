# $NetBSD: depsrc-usebefore.mk,v 1.5 2020/08/22 11:53:18 rillig Exp $
#
# Tests for the special source .USEBEFORE in dependency declarations,
# which allows to prepend common commands to other targets.

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
