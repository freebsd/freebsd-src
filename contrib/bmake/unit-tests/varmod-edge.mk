# $NetBSD: varmod-edge.mk,v 1.37 2025/06/28 22:39:29 rillig Exp $
#
# Tests for edge cases in variable modifiers.
#
# These tests demonstrate the current implementation in small examples.
# They may contain surprising behavior.
#
# Each test consists of:
# - INP, the input to the test
# - MOD, the expression for testing the modifier
# - EXP, the expected output

INP=	(parentheses) {braces} (opening closing) ()
MOD=	${INP:M(*)}
EXP=	(parentheses) ()
.if ${MOD} != ${EXP}
.  warning expected "${EXP}", got "${MOD}"
.endif

# The first closing brace matches the opening parenthesis.
# The second closing brace actually ends the expression.
#
# XXX: This is unexpected but rarely occurs in practice.
INP=	(paren-brace} (
MOD=	${INP:M(*}}
EXP=	(paren-brace}
.if ${MOD} != ${EXP}
.  warning expected "${EXP}", got "${MOD}"
.endif

# After the :M modifier has parsed the pattern, only the closing brace
# and the colon are unescaped. The other characters are left as-is.
# To actually see this effect, the backslashes in the :M modifier need
# to be doubled since single backslashes would simply be unescaped by
# Str_Match.
#
# XXX: This is unexpected. The opening brace should also be unescaped.
INP=	({}): \(\{\}\)\: \(\{}\):
MOD=	${INP:M\\(\\{\\}\\)\\:}
EXP=	\(\{}\):
.if ${MOD} != ${EXP}
.  warning expected "${EXP}", got "${MOD}"
.endif

# When the :M and :N modifiers are parsed, the pattern finishes as soon
# as open_parens + open_braces == closing_parens + closing_braces. This
# means that ( and } form a matching pair.
#
# Nested expressions are not parsed as such. Instead, only the
# parentheses and braces are counted. This leads to a parse error since
# the nested expression is not "${:U*)}" but only "${:U*)", which is
# missing the closing brace. The expression is evaluated anyway.
# The final brace in the output comes from the end of M.nest-mix.
#
# XXX: This is unexpected but rarely occurs in practice.
INP=	(parentheses)
MOD=	${INP:M${:U*)}}
EXP=	(parentheses)}
# expect+1: Unclosed expression, expecting "}" for modifier "U*)"
.if ${MOD} != ${EXP}
.  warning expected "${EXP}", got "${MOD}"
.endif


# In contrast to parentheses and braces, the brackets are not counted
# when the :M modifier is parsed since Makefile expressions only take the
# ${VAR} or $(VAR) forms, but not $[VAR].
#
# The final ] in the pattern is needed to close the character class.
INP=	[ [[ [[[
MOD=	${INP:M${:U[[[[[]}}
EXP=	[
.if ${MOD} != ${EXP}
.  warning expected "${EXP}", got "${MOD}"
.endif


# The pattern in the nested variable has an unclosed character class.
#
# Before str.c 1.104 from 2024-07-06, no error was reported.
#
# Before 2019-12-02, this test case triggered an out-of-bounds read
# in Str_Match.
INP=	[ [[ [[[
MOD=	${INP:M${:U[[}}
EXP=	[
# expect+1: Unfinished character list in pattern "[[" of modifier ":M"
.if ${MOD} != ${EXP}
.  warning expected "${EXP}", got "${MOD}"
.endif

# The first backslash does not escape the second backslash.
# Therefore, the second backslash escapes the parenthesis.
# This means that the pattern ends there.
# The final } in the output comes from the end of MOD.
#
# If the first backslash were to escape the second backslash, the first
# closing brace would match the opening parenthesis (see paren-brace), and
# the second closing brace would be needed to close the variable.
# After that, the remaining backslash would escape the parenthesis in
# the pattern, therefore (} would match.
INP=	(} \( \(}
MOD=	${INP:M\\(}}
EXP=	\(}
#EXP=	(}	# If the first backslash were to escape ...
.if ${MOD} != ${EXP}
.  warning expected "${EXP}", got "${MOD}"
.endif

# The backslash in \( does not escape the parenthesis, therefore it
# counts for the nesting level and matches with the first closing brace.
# The second closing brace closes the variable, and the third is copied
# literally.
#
# The second :M in the pattern is nested between ( and }, therefore it
# does not start a new modifier.
INP=	( (:M (:M} \( \(:M \(:M}
MOD=	${INP:M\(:M*}}}
EXP=	(:M}}
.if ${MOD} != ${EXP}
.  warning expected "${EXP}", got "${MOD}"
.endif

# The double backslash is passed verbatim to the pattern matcher.
# The Str_Match pattern is \\(:M*}, and there the backslash is unescaped.
# Again, the ( takes place in the nesting level, and there is no way to
# prevent this, no matter how many backslashes are used.
INP=	( (:M (:M} \( \(:M \(:M}
MOD=	${INP:M\\(:M*}}}
EXP=	\(:M}}
.if ${MOD} != ${EXP}
.  warning expected "${EXP}", got "${MOD}"
.endif

# Before str.c 1.48 from 2020-06-15, Str_Match used a recursive algorithm for
# matching the '*' patterns and did not optimize for multiple '*' in a row.
# Test a pattern with 65536 asterisks.
INP=	${:U\\:Q:Q:Q:Q:Q:Q:Q:Q:Q:Q:Q:Q:Q:Q:Q:Q:S,\\,x,g}
PAT=	${:U\\:Q:Q:Q:Q:Q:Q:Q:Q:Q:Q:Q:Q:Q:Q:Q:Q:S,\\,*,g}
MOD=	${INP:M${PAT}}
EXP=	${INP}
.if ${MOD} != ${EXP}
.  warning expected "${EXP}", got "${MOD}"
.endif

# This is the normal SysV substitution. Nothing surprising here.
INP=	file.c file.cc
MOD=	${INP:%.c=%.o}
EXP=	file.o file.cc
.if ${MOD} != ${EXP}
.  warning expected "${EXP}", got "${MOD}"
.endif

# The SysV := modifier is greedy and consumes all the modifier text
# up until the closing brace or parenthesis. The :Q may look like a
# modifier, but it really isn't, that's why it appears in the output.
INP=	file.c file.cc
MOD=	${INP:%.c=%.o:Q}
EXP=	file.o:Q file.cc
.if ${MOD} != ${EXP}
.  warning expected "${EXP}", got "${MOD}"
.endif

# The = in the := modifier can be escaped.
INP=	file.c file.c=%.o
MOD=	${INP:%.c\=%.o=%.ext}
EXP=	file.c file.ext
.if ${MOD} != ${EXP}
.  warning expected "${EXP}", got "${MOD}"
.endif

# Having only an escaped '=' results in a parse error.
# The call to "pattern.lhs = ParseModifierPart" fails.
INP=	file.c file...
MOD=	${INP:a\=b}
EXP=	# empty
# expect+1: Unfinished modifier after "a\=b}", expecting "="
.if ${MOD} != ${EXP}
.  warning expected "${EXP}", got "${MOD}"
.endif

INP=	value
MOD=	${INP:}
EXP=	value
.if ${MOD} != ${EXP}
.  warning expected "${EXP}", got "${MOD}"
.endif

INP=	value
MOD=	${INP::::}
EXP=	:}
# expect+1: Unknown modifier "::"
.if ${MOD} != ${EXP}
.  warning expected "${EXP}", got "${MOD}"
.endif

# Even in expressions based on an unnamed variable, there may be errors.
# expect+1: Unknown modifier ":Z"
.if ${:Z}
.  error
.else
.  error
.endif

# Even in expressions based on an unnamed variable, there may be errors.
#
# Before var.c 1.842 from 2021-02-23, the error message did not surround the
# variable name with quotes, leading to the rather confusing "Unfinished
# modifier for  (',' missing)", having two spaces in a row.
#
# expect+1: Unfinished modifier after "}", expecting ","
.if ${:S,}
.  error
.else
.  error
.endif
