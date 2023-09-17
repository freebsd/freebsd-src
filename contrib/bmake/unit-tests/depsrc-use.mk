# $NetBSD: depsrc-use.mk,v 1.6 2022/04/18 14:38:24 rillig Exp $
#
# Tests for the special source .USE in dependency declarations,
# which allows to append common commands to other targets.
#
# See also:
#	.USEBEFORE
#	depsrc-usebefore.mk

# Before make.h 1.280 from 2021-12-28, a .USEBEFORE target was accidentally
# regarded as a candidate for the main target.  On the other hand, a .USE
# target was not.
not-a-main-candidate: .USE

all: action directly

first: .USE first-first first-second
	@echo first 1		# Using ${.TARGET} here would expand to "action"
	@echo first 2
first-first: .USE
	@echo first-first 1
	@echo first-first 2
first-second: .USE
	@echo first-second 1
	@echo first-second 2

second: .USE
	@echo second 1
	@echo second 2

# It's possible but uncommon to have a .USE target with no commands.
# This may happen as the result of expanding a .for loop.
empty: .USE

# It's possible but uncommon to directly make a .USE target.
directly: .USE
	@echo directly

action: first second empty
