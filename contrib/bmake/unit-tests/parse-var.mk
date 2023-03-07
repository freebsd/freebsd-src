# $NetBSD: parse-var.mk,v 1.6 2022/09/25 21:26:23 rillig Exp $
#
# Tests for parsing variable expressions.
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
#	eval
#	eval-undeferr
#	eval-keep-dollar
#	eval-keep-undef
#	eval-keep-dollar-undef
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
#	What's the value of the expression (out_val)?
#	What's the status after parsing the expression (VarParseResult)?
#	What error messages are printed (Parse_Error)?
#	What no-effect error messages are printed (Error)?
#	What error messages should be printed but aren't?
#	What other side effects are there?

.MAKEFLAGS: -dL

# In variable assignments, there may be spaces in the middle of the left-hand
# side of the assignment, but only if they occur inside variable expressions.
# Leading spaces (but not tabs) are possible but unusual.
# Trailing spaces are common in some coding styles, others omit them.
VAR.${:U param }=	value
.if ${VAR.${:U param }} != "value"
.  error
.endif

# XXX: The following paragraph already uses past tense, in the hope that the
# parsing behavior can be cleaned up soon.

# Since var.c 1.323 from 2020-07-26 18:11 and except for var.c 1.1028 from
# 2022-08-08, the exact way of parsing an expression depended on whether the
# expression was actually evaluated or merely parsed.
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
#
# This naive brace counting was implemented in ParseModifierPartDollar.  As of
# var.c 1.1029, there are still several other places that merely count braces
# instead of properly parsing subexpressions.

#.MAKEFLAGS: -dcpv
# Keep these braces outside the conditions below, to keep them simple to
# understand.  If the BRACE_PAIR had been replaced with ':U{}', the '}' would
# have to be escaped, but not the '{'.  This asymmetry would have made the
# example even more complicated to understand.
BRACE_PAIR=	{}
# In this test word, the '{{}' in the middle will be replaced.
BRACE_GROUP=	{{{{}}}}

# The inner ':S' modifier turns the word '{}' into '{{}'.
# The outer ':S' modifier then replaces '{{}' with '<lbraces>'.
# In the first case, the outer expression is relevant and is parsed correctly.
.if 1 && ${BRACE_GROUP:S,${BRACE_PAIR:S,{,{{,},<lbraces>,}
.endif
# In the second case, the outer expression was irrelevant.  In this case, in
# the parts of the outer ':S' modifier, make only counted the braces, and since
# the inner expression '${BRACE_PAIR:...}' contains more '{' than '}', parsing
# failed with the error message 'Unfinished modifier for "BRACE_GROUP"'.  Fixed
# in var.c 1.1028 from 2022-08-08, reverted in var.c 1.1029 from 2022-08-23.
.if 0 && ${BRACE_GROUP:S,${BRACE_PAIR:S,{,{{,},<lbraces>,}
.endif
#.MAKEFLAGS: -d0


all: .PHONY
