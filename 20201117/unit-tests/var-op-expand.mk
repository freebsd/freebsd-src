# $NetBSD: var-op-expand.mk,v 1.4 2020/11/08 14:00:52 rillig Exp $
#
# Tests for the := variable assignment operator, which expands its
# right-hand side.

# TODO: Implementation

# XXX: edge case: When a variable name refers to an undefined variable, the
# behavior differs between the '=' and the ':=' assignment operators.
# This bug exists since var.c 1.42 from 2000-05-11.
#
# The '=' operator expands the undefined variable to an empty string, thus
# assigning to VAR_ASSIGN_.  In the name of variables to be set, it should
# really be forbidden to refer to undefined variables.
#
# The ':=' operator expands the variable name twice.  In one of these
# expansions, the undefined variable expression is preserved (controlled by
# preserveUndefined in VarAssign_EvalSubst), in the other expansion it expands
# to an empty string.  This way, 2 variables are created using a single
# variable assignment.  It's magic. :-/
.MAKEFLAGS: -dv
VAR_ASSIGN_${UNDEF}=	undef value
VAR_SUBST_${UNDEF}:=	undef value
.MAKEFLAGS: -d0

all:
	@:;
