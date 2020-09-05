# $NetBSD: dep-var.mk,v 1.1 2020/08/22 16:51:26 rillig Exp $
#
# Tests for variable references in dependency declarations.
#
# Uh oh, this feels so strange that probably nobody uses it. But it seems to
# be the only way to reach the lower half of SuffExpandChildren.

# XXX: The -dv log says:
#	Var_Parse: ${UNDEF1} with VARE_UNDEFERR|VARE_WANTRES
# but no error message is generated for this line.
# The variable expression ${UNDEF1} simply expands to an empty string.
all: ${UNDEF1}

# Using a double dollar in order to circumvent immediate variable expansion
# feels like unintended behavior.  At least the manual page says nothing at
# all about defined or undefined variables in dependency lines.
#
# At the point where the expression ${DEF2} is expanded, the variable DEF2
# is defined, so everything's fine.
all: $${DEF2}

# This variable is not defined at all.
# XXX: The -dv log says:
#	Var_Parse: ${UNDEF3} with VARE_UNDEFERR|VARE_WANTRES
# but no error message is generated for this line, just like for UNDEF1.
# The variable expression ${UNDEF3} simply expands to an empty string.
all: $${UNDEF3}

UNDEF1=	undef1
DEF2=	def2

undef1 def2:
	@echo ${.TARGET}
