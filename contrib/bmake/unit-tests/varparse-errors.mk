# $NetBSD: varparse-errors.mk,v 1.26 2025/06/28 22:39:29 rillig Exp $

# Tests for parsing and evaluating all kinds of expressions.
#
# This is the basis for redesigning the error handling in Var_Parse and
# Var_Subst, collecting typical and not so typical use cases.
#
# See also:
#	Var_Parse
#	Var_Subst

PLAIN=		plain value

LITERAL_DOLLAR=	To get a dollar, double $$ it.

INDIRECT=	An ${:Uindirect} value.

REF_UNDEF=	A reference to an ${UNDEF}undefined variable.

ERR_UNCLOSED=	An ${UNCLOSED expression.

ERR_BAD_MOD=	An ${:Uindirect:Z} expression with an unknown modifier.

ERR_EVAL=	An evaluation error ${:Uvalue:C,.,\3,}.

# In a conditional, an expression that is not enclosed in quotes is
# expanded using the mode VARE_EVAL_DEFINED.
# The variable itself must be defined.
# It may refer to undefined variables though.
.if ${REF_UNDEF} != "A reference to an undefined variable."
.  error
.endif

# As of 2020-12-01, errors in the variable name are silently ignored.
# Since var.c 1.754 from 2020-12-20, unknown modifiers at parse time result
# in an error message and a non-zero exit status.
# expect+1: Unknown modifier ":Z"
VAR.${:U:Z}=	unknown modifier in the variable name
.if ${VAR.} != "unknown modifier in the variable name"
.  error
.endif

# As of 2020-12-01, errors in the variable name are silently ignored.
# Since var.c 1.754 from 2020-12-20, unknown modifiers at parse time result
# in an error message and a non-zero exit status.
# expect+1: Unknown modifier ":Z"
VAR.${:U:Z}post=	unknown modifier with text in the variable name
.if ${VAR.post} != "unknown modifier with text in the variable name"
.  error
.endif

# Demonstrate an edge case in which the 'static' for 'errorReported' in
# Var_Subst actually makes a difference, preventing "a plethora of messages".
# Given that this is an edge case and the error message is wrong and thus
# misleading anyway, that piece of code is probably not necessary.  The wrong
# condition was added in var.c 1.185 from 2014-05-19.
#
# To trigger this difference, the variable assignment must use the assignment
# operator ':=' to make VarEvalMode_ShouldKeepUndef return true.  There must
# be 2 expressions that create a parse error, which in this case is ':OX'.
# These expressions must be nested in some way.  The below expressions are
# minimal, that is, removing any part of it destroys the effect.
#
# Without the 'static', there would be one more message like this:
#	Undefined variable "${:U:OX"
#
#.MAKEFLAGS: -dv
IND=	${:OX}
# expect+4: Unknown modifier ":OX"
# expect+3: Unknown modifier ":OX"
# expect+2: Unknown modifier ":OX"
# expect+1: Unknown modifier ":OX"
_:=	${:U:OX:U${IND}} ${:U:OX:U${IND}}
#.MAKEFLAGS: -d0


# Before var.c 1.032 from 2022-08-24, make complained about 'Unknown modifier'
# or 'Bad modifier' when in fact the modifier was entirely correct, it was
# just not delimited by either ':' or '}' but instead by '\0'.
# expect+1: Unclosed expression, expecting "}" for modifier "Q"
UNCLOSED:=	${:U:Q
# expect+1: Unclosed expression, expecting "}" for modifier "sh"
UNCLOSED:=	${:U:sh
# expect+1: Unclosed expression, expecting "}" for modifier "tA"
UNCLOSED:=	${:U:tA
# expect+1: Unclosed expression, expecting "}" for modifier "tsX"
UNCLOSED:=	${:U:tsX
# expect+1: Unclosed expression, expecting "}" for modifier "ts"
UNCLOSED:=	${:U:ts
# expect+1: Unclosed expression, expecting "}" for modifier "ts\040"
UNCLOSED:=	${:U:ts\040
# expect+1: Unclosed expression, expecting "}" for modifier "u"
UNCLOSED:=	${:U:u
# expect+1: Unclosed expression, expecting "}" for modifier "H"
UNCLOSED:=	${:U:H
# expect+1: Unclosed expression, expecting "}" for modifier "[1]"
UNCLOSED:=	${:U:[1]
# expect+1: Unclosed expression, expecting "}" for modifier "hash"
UNCLOSED:=	${:U:hash
# expect+1: Unclosed expression, expecting "}" for modifier "range"
UNCLOSED:=	${:U:range
# expect+1: Unclosed expression, expecting "}" for modifier "_"
UNCLOSED:=	${:U:_
# expect+1: Unclosed expression, expecting "}" for modifier "gmtime"
UNCLOSED:=	${:U:gmtime
# expect+1: Unclosed expression, expecting "}" for modifier "localtime"
UNCLOSED:=	${:U:localtime


# In a stack trace that has both evaluation details and included files, list
# the current file twice: Once in the first line and once in the call
# hierarchy. While this is redundant, omitting the current file from the
# call hierarchy is more confusing, as the '.include' line does not contain
# the faulty expression.
#
# expect: make: varparse-errors.tmp:1: Unknown modifier ":Z"
# expect:	while evaluating "${:Z}" with value ""
# expect:	while evaluating variable "INDIRECT" with value "${:Z}"
# expect:	while evaluating variable "VALUE" with value "${INDIRECT}"
# expect:	in varparse-errors.tmp:1
# expect:	in varparse-errors.mk:126
_!=	echo '.info $${VALUE}' > varparse-errors.tmp
VALUE=	${INDIRECT}
INDIRECT=	${:Z}
# The "${.OBJDIR}/" is necessary to bypass the directory cache.
.include "${.OBJDIR}/varparse-errors.tmp"
_!=	rm -f varparse-errors.tmp
