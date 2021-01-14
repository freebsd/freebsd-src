# $NetBSD: depsrc.mk,v 1.4 2020/12/22 19:38:44 rillig Exp $
#
# Tests for special sources (those starting with a dot, followed by
# uppercase letters) in dependency declarations, such as .PHONY.

# TODO: Implementation

# TODO: Test 'target: ${:U.SILENT}'

# Demonstrate when exactly undefined variables are expanded in a dependency
# declaration.
target: .PHONY source-${DEFINED_LATER}
#
DEFINED_LATER=	later
#
source-: .PHONY
	: 'Undefined variables are expanded directly in the dependency'
	: 'declaration.  They are not preserved and maybe expanded later.'
	: 'This is in contrast to local variables such as $${.TARGET}.'
source-later: .PHONY
	: 'Undefined variables are tried to be expanded in a dependency'
	: 'declaration.  If that fails because the variable is undefined,'
	: 'the expression is preserved and tried to be expanded later.'

all:
	@:;
