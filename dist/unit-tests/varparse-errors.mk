# $NetBSD: varparse-errors.mk,v 1.1 2020/11/08 16:44:47 rillig Exp $

# Tests for parsing and evaluating all kinds of variable expressions.
#
# This is the basis for redesigning the error handling in Var_Parse and
# Var_Subst, collecting typical and not so typical use cases.
#
# See also:
#	VarParseResult
#	Var_Parse
#	Var_Subst

PLAIN=		plain value

LITERAL_DOLLAR=	To get a dollar, double $$ it.

INDIRECT=	An ${:Uindirect} value.

REF_UNDEF=	A reference to an ${UNDEF}undefined variable.

ERR_UNCLOSED=	An ${UNCLOSED variable expression.

ERR_BAD_MOD=	An ${:Uindirect:Z} expression with an unknown modifier.

ERR_EVAL=	An evaluation error ${:Uvalue:C,.,\3,}.

# In a conditional, a variable expression that is not enclosed in quotes is
# expanded using the flags VARE_UNDEFERR and VARE_WANTRES.
# The variable itself must be defined.
# It may refer to undefined variables though.
.if ${REF_UNDEF} != "A reference to an undefined variable."
.  error
.endif

all:
