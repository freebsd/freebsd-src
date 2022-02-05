# $NetBSD: depsrc-use.mk,v 1.5 2021/12/28 14:22:51 rillig Exp $
#
# Tests for the special source .USE in dependency declarations,
# which allows to append common commands to other targets.

# Before make.h 1.280 from 2021-12-28, a .USEBEFORE target was accidentally
# regarded as a candidate for the main target.  On the other hand, a .USE
# target was not.
not-a-main-candidate: .USE

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
