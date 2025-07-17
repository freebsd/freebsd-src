# $NetBSD: depsrc-usebefore.mk,v 1.9 2022/04/18 14:41:42 rillig Exp $
#
# Tests for the special source .USEBEFORE in dependency declarations,
# which allows to prepend common commands to other targets.
#
# If a target depends on several .USE or .USEBEFORE nodes, the commands get
# appended or prepended in declaration order.  For .USE nodes, this is the
# expected order, for .USEBEFORE nodes the order is somewhat reversed, and for
# .USE or .USEBEFORE nodes that depend on other .USE or .USEBEFORE nodes, it
# gets even more complicated.
#
# See also:
#	.USE
#	depsrc-use.mk

# Before make.h 1.280 from 2021-12-28, a .USEBEFORE target was accidentally
# regarded as a candidate for the main target.  On the other hand, a .USE
# target was not.
not-a-main-candidate: .USEBEFORE

all:
	@${MAKE} -r -f ${MAKEFILE} ordering
	@${MAKE} -r -f ${MAKEFILE} directly

ordering: before-1 before-2 after-1 after-2

before-1: .USEBEFORE before-1-before-1 before-1-before-2 before-1-after-1 before-1-after-2
	@echo before-1 1
	@echo before-1 2

before-1-before-1: .USEBEFORE
	@echo before-1-before-1 1
	@echo before-1-before-1 2

before-1-before-2: .USEBEFORE
	@echo before-1-before-2 1
	@echo before-1-before-2 2

before-1-after-1: .USE
	@echo before-1-after-1 1
	@echo before-1-after-1 2

before-1-after-2: .USE
	@echo before-1-after-2 1
	@echo before-1-after-2 2

before-2: .USEBEFORE before-2-before-1 before-2-before-2 before-2-after-1 before-2-after-2
	@echo before-2 1
	@echo before-2 2

before-2-before-1: .USEBEFORE
	@echo before-2-before-1 1
	@echo before-2-before-1 2

before-2-before-2: .USEBEFORE
	@echo before-2-before-2 1
	@echo before-2-before-2 2

before-2-after-1: .USE
	@echo before-2-after-1 1
	@echo before-2-after-1 2

before-2-after-2: .USE
	@echo before-2-after-2 1
	@echo before-2-after-2 2

after-1: .USE after-1-before-1 after-1-before-2 after-1-after-1 after-1-after-2
	@echo after-1 1
	@echo after-1 2

after-1-before-1: .USEBEFORE
	@echo after-1-before-1 1
	@echo after-1-before-1 2

after-1-before-2: .USEBEFORE
	@echo after-1-before-2 1
	@echo after-1-before-2 2

after-1-after-1: .USE
	@echo after-1-after-1 1
	@echo after-1-after-1 2

after-1-after-2: .USE
	@echo after-1-after-2 1
	@echo after-1-after-2 2

after-2: .USE after-2-before-1 after-2-before-2 after-2-after-1 after-2-after-2
	@echo after-2 1
	@echo after-2 2

after-2-before-1: .USEBEFORE
	@echo after-2-before-1 1
	@echo after-2-before-1 2

after-2-before-2: .USEBEFORE
	@echo after-2-before-2 1
	@echo after-2-before-2 2

after-2-after-1: .USE
	@echo after-2-after-1 1
	@echo after-2-after-1 2

after-2-after-2: .USE
	@echo after-2-after-2 1
	@echo after-2-after-2 2

# It is possible but uncommon to have a .USEBEFORE target with no commands.
# This may happen as the result of expanding a .for loop.
empty: .USEBEFORE

# It is technically possible to directly make a .USEBEFORE target, but it
# doesn't make sense since GNode_IsOODate considers such a target to always be
# up to date.
directly: .USEBEFORE
	@echo directly
