# $NetBSD: depsrc-use.mk,v 1.4 2020/08/22 12:30:57 rillig Exp $
#
# Tests for the special source .USE in dependency declarations,
# which allows to append common commands to other targets.

all: action directly

first: .USE
	@echo first 1		# Using ${.TARGET} here would expand to "action"
	@echo first 2

second: .USE
	@echo second 1
	@echo second 2

# It's possible but uncommon to have a .USE target with no commands.
# This may happen as the result of expanding a .for loop.
empty: .USE

# It's possible but uncommon to directly make a .USEBEFORE target.
directly: .USE
	@echo directly

action: first second empty
