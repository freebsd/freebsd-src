# $NetBSD: depsrc.mk,v 1.5 2021/12/13 23:38:54 rillig Exp $
#
# Tests for special sources (those starting with a dot, followed by
# uppercase letters) in dependency declarations, such as '.PHONY'.

# TODO: Implementation

# TODO: Test 'target: ${:U.SILENT}'

# Demonstrate when exactly undefined variables are expanded in a dependency
# declaration.
target: .PHONY source-${DEFINED_LATER}
#
DEFINED_LATER=	later
#
source-: .PHONY
	# This section applies.
	: 'Undefined variables are expanded directly in the dependency'
	: 'declaration.  They are not preserved and maybe expanded later.'
	: 'This is in contrast to local variables such as $${.TARGET}.'
source-later: .PHONY
	# This section doesn't apply.
	: 'Undefined variables are tried to be expanded in a dependency'
	: 'declaration.  If that fails because the variable is undefined,'
	: 'the expression is preserved and tried to be expanded later.'

# Sources that look like keywords but are not known are interpreted as
# ordinary sources.
target: .UNKNOWN

.UNKNOWN:
	: Making ${.TARGET} from ${.ALLSRC:S,^$,nothing,W}.
