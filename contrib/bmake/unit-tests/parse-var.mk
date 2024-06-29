# $NetBSD: parse-var.mk,v 1.10 2024/06/02 15:31:26 rillig Exp $
#
# Tests for parsing expressions.
#
# TODO: Add systematic tests for all of the below combinations.
#
# Written form:
#	short form
#	long form with braces		endc == '}'
#	long form with parentheses	endc == ')'
#	indirect modifiers		endc == '\0'
#
# Based on:
#	undefined variable
#	global variable
#	command-line variable
#	environment variable
#	target-local variable
#	legacy variable '@F'
#
# VarEvalMode:
#	parse
#	parse-balanced
#	eval
#	eval-defined
#	eval-keep-undefined
#	eval-keep-dollar-and-undefined
#
# Global mode:
#	without -dL
#	with -dL
#
# Modifiers:
#	no
#	yes, stay undefined
#	convert to defined
#	indirect modifiers, involving changes to VarEvalMode
#
# Error conditions:
#	for the short form, EOF after the '$'
#	for the short form, each character
#	for the long forms, EOF right after '${'
#	for the long forms, EOF after the variable name
#	for the long forms, EOF after the ':'
#	for the long forms, EOF after parsing a modifier
#	for the long forms, ':}'
#	for each modifier: syntactic error
#	for each modifier: evaluation error
#
# Context:
#	in a condition, only operand, unquoted
#	in a condition, only operand, quoted
#	in a condition, left-hand side, unquoted
#	in a condition, left-hand side, quoted
#	in a condition, right-hand side, unquoted
#	in a condition, right-hand side, quoted
#	left-hand side of a variable assignment
#	right-hand side of a ':=' variable assignment
#	right-hand side of a '!=' variable assignment
#	shell command in a target
#	.info directive
#	dependency line
#	items in a .for loop
#	everywhere else Var_Parse is called
#
# Further influences:
#	multi-level evaluations like 'other=${OTHER}' with OTHER='$$ ${THIRD}'
#
# Effects:
#	How much does the parsing position advance (pp)?
#	What's the value of the expression (return value)?
#	What error messages are printed (Parse_Error)?
#	What no-effect error messages are printed (Error)?
#	What error messages should be printed but aren't?
#	What other side effects are there?

.MAKEFLAGS: -dL

# In variable assignments, there may be spaces in the middle of the left-hand
# side of the assignment, but only if they occur inside expressions.
# Leading spaces (but not tabs) are possible but unusual.
# Trailing spaces are common in some coding styles, others omit them.
VAR.${:U param }=	value
.if ${VAR.${:U param }} != "value"
.  error
.endif

# Since var.c 1.323 from 2020-07-26 18:11 and until var.c 1.1047 from
# 2023-02-18, the exact way of parsing an expression with subexpressions
# depended on whether the expression was actually evaluated or merely parsed.
#
# If it was evaluated, nested expressions were parsed correctly, parsing each
# modifier according to its exact definition (see varmod.mk).
#
# If the expression was merely parsed but not evaluated (for example, because
# its value would not influence the outcome of the condition, or during the
# first pass of the ':@var@body@' modifier), and the expression contained a
# modifier, and that modifier contained a nested expression, the nested
# expression was not parsed correctly.  Instead, make only counted the opening
# and closing delimiters, which failed for nested modifiers with unbalanced
# braces.

#.MAKEFLAGS: -dcpv
# Keep these braces outside the conditions below, to keep them simple to
# understand.  If the expression ${BRACE_PAIR:...} had been replaced with the
# literal ${:U{}}, the '}' would have to be escaped, but not the '{'.  This
# asymmetry would have made the example even more complicated to understand.
BRACE_PAIR=	{}
# In this test word, the below conditions will replace the '{{}' in the middle
# with the string '<lbraces>'.
BRACE_GROUP=	{{{{}}}}

# The inner ':S' modifier turns the word '{}' into '{{}'.
# The outer ':S' modifier then replaces '{{}' with '<lbraces>'.
# Due to the always-true condition '1', the outer expression is relevant and
# is parsed correctly.
.if 1 && ${BRACE_GROUP:S,${BRACE_PAIR:S,{,{{,},<lbraces>,}
.endif
# Due to the always-false condition '0', the outer expression is irrelevant.
# In this case, in the parts of the outer ':S' modifier, the expression parser
# only counted the braces, and since the inner expression '${BRACE_PAIR:...}'
# contains more '{' than '}', parsing failed with the error message 'Unfinished
# modifier for "BRACE_GROUP"'.  Fixed in var.c 1.1047 from 2023-02-18.
.if 0 && ${BRACE_GROUP:S,${BRACE_PAIR:S,{,{{,},<lbraces>,}
.endif
#.MAKEFLAGS: -d0


all: .PHONY
