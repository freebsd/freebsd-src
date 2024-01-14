# $NetBSD: varmod-edge.mk,v 1.19 2023/11/19 22:06:15 rillig Exp $
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

TESTS+=		M-paren
INP.M-paren=	(parentheses) {braces} (opening closing) ()
MOD.M-paren=	${INP.M-paren:M(*)}
EXP.M-paren=	(parentheses) ()

# The first closing brace matches the opening parenthesis.
# The second closing brace actually ends the expression.
#
# XXX: This is unexpected but rarely occurs in practice.
TESTS+=		M-mixed
INP.M-mixed=	(paren-brace} (
MOD.M-mixed=	${INP.M-mixed:M(*}}
EXP.M-mixed=	(paren-brace}

# After the :M modifier has parsed the pattern, only the closing brace
# and the colon are unescaped. The other characters are left as-is.
# To actually see this effect, the backslashes in the :M modifier need
# to be doubled since single backslashes would simply be unescaped by
# Str_Match.
#
# XXX: This is unexpected. The opening brace should also be unescaped.
TESTS+=		M-unescape
INP.M-unescape=	({}): \(\{\}\)\: \(\{}\):
MOD.M-unescape=	${INP.M-unescape:M\\(\\{\\}\\)\\:}
EXP.M-unescape=	\(\{}\):

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
TESTS+=		M-nest-mix
INP.M-nest-mix=	(parentheses)
MOD.M-nest-mix=	${INP.M-nest-mix:M${:U*)}}
EXP.M-nest-mix=	(parentheses)}
# make: Unclosed expression, expecting '}' for modifier "U*)" of variable "" with value "*)"

# In contrast to parentheses and braces, the brackets are not counted
# when the :M modifier is parsed since Makefile variables only take the
# ${VAR} or $(VAR) forms, but not $[VAR].
#
# The final ] in the pattern is needed to close the character class.
TESTS+=		M-nest-brk
INP.M-nest-brk=	[ [[ [[[
MOD.M-nest-brk=	${INP.M-nest-brk:M${:U[[[[[]}}
EXP.M-nest-brk=	[

# The pattern in the nested variable has an unclosed character class.
# No error is reported though, and the pattern is closed implicitly.
#
# XXX: It is unexpected that no error is reported.
# See str.c, function Str_Match.
#
# Before 2019-12-02, this test case triggered an out-of-bounds read
# in Str_Match.
TESTS+=		M-pat-err
INP.M-pat-err=	[ [[ [[[
MOD.M-pat-err=	${INP.M-pat-err:M${:U[[}}
EXP.M-pat-err=	[

# The first backslash does not escape the second backslash.
# Therefore, the second backslash escapes the parenthesis.
# This means that the pattern ends there.
# The final } in the output comes from the end of MOD.M-bsbs.
#
# If the first backslash were to escape the second backslash, the first
# closing brace would match the opening parenthesis (see M-mixed), and
# the second closing brace would be needed to close the variable.
# After that, the remaining backslash would escape the parenthesis in
# the pattern, therefore (} would match.
TESTS+=		M-bsbs
INP.M-bsbs=	(} \( \(}
MOD.M-bsbs=	${INP.M-bsbs:M\\(}}
EXP.M-bsbs=	\(}
#EXP.M-bsbs=	(}	# If the first backslash were to escape ...

# The backslash in \( does not escape the parenthesis, therefore it
# counts for the nesting level and matches with the first closing brace.
# The second closing brace closes the variable, and the third is copied
# literally.
#
# The second :M in the pattern is nested between ( and }, therefore it
# does not start a new modifier.
TESTS+=		M-bs1-par
INP.M-bs1-par=	( (:M (:M} \( \(:M \(:M}
MOD.M-bs1-par=	${INP.M-bs1-par:M\(:M*}}}
EXP.M-bs1-par=	(:M}}

# The double backslash is passed verbatim to the pattern matcher.
# The Str_Match pattern is \\(:M*}, and there the backslash is unescaped.
# Again, the ( takes place in the nesting level, and there is no way to
# prevent this, no matter how many backslashes are used.
TESTS+=		M-bs2-par
INP.M-bs2-par=	( (:M (:M} \( \(:M \(:M}
MOD.M-bs2-par=	${INP.M-bs2-par:M\\(:M*}}}
EXP.M-bs2-par=	\(:M}}

# Str_Match uses a recursive algorithm for matching the * patterns.
# Make sure that it survives patterns with 128 asterisks.
# That should be enough for all practical purposes.
# To produce a stack overflow, just add more :Qs below.
TESTS+=		M-128
INP.M-128=	${:U\\:Q:Q:Q:Q:Q:Q:Q:S,\\,x,g}
PAT.M-128=	${:U\\:Q:Q:Q:Q:Q:Q:Q:S,\\,*,g}
MOD.M-128=	${INP.M-128:M${PAT.M-128}}
EXP.M-128=	${INP.M-128}

# This is the normal SysV substitution. Nothing surprising here.
TESTS+=		eq-ext
INP.eq-ext=	file.c file.cc
MOD.eq-ext=	${INP.eq-ext:%.c=%.o}
EXP.eq-ext=	file.o file.cc

# The SysV := modifier is greedy and consumes all the modifier text
# up until the closing brace or parenthesis. The :Q may look like a
# modifier, but it really isn't, that's why it appears in the output.
TESTS+=		eq-q
INP.eq-q=	file.c file.cc
MOD.eq-q=	${INP.eq-q:%.c=%.o:Q}
EXP.eq-q=	file.o:Q file.cc

# The = in the := modifier can be escaped.
TESTS+=		eq-bs
INP.eq-bs=	file.c file.c=%.o
MOD.eq-bs=	${INP.eq-bs:%.c\=%.o=%.ext}
EXP.eq-bs=	file.c file.ext

# Having only an escaped '=' results in a parse error.
# The call to "pattern.lhs = ParseModifierPart" fails.
TESTS+=		eq-esc
INP.eq-esc=	file.c file...
MOD.eq-esc=	${INP.eq-esc:a\=b}
EXP.eq-esc=	# empty
# make: Unfinished modifier for INP.eq-esc ('=' missing)

TESTS+=		colon
INP.colon=	value
MOD.colon=	${INP.colon:}
EXP.colon=	value

TESTS+=		colons
INP.colons=	value
MOD.colons=	${INP.colons::::}
EXP.colons=	# empty

.for test in ${TESTS}
# expect+2: Unknown modifier ":"
# expect+1: Unknown modifier ":"
.  if ${MOD.${test}} == ${EXP.${test}}
# expect+16: ok M-paren
# expect+15: ok M-mixed
# expect+14: ok M-unescape
# expect+13: ok M-nest-mix
# expect+12: ok M-nest-brk
# expect+11: ok M-pat-err
# expect+10: ok M-bsbs
# expect+09: ok M-bs1-par
# expect+08: ok M-bs2-par
# expect+07: ok M-128
# expect+06: ok eq-ext
# expect+05: ok eq-q
# expect+04: ok eq-bs
# expect+03: ok eq-esc
# expect+02: ok colon
# expect+01: ok colons
.    info ok ${test}
.  else
.    warning error in ${test}: expected "${EXP.${test}}", got "${MOD.${test}}"
.  endif
.endfor

# Even in expressions based on an unnamed variable, there may be errors.
# XXX: The error message should mention the variable name of the expression,
# even though that name is empty in this case.
# expect+2: Malformed conditional (${:Z})
# expect+1: Unknown modifier "Z"
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
# XXX: The error message should report the filename:lineno.
# expect+1: Malformed conditional (${:S,})
.if ${:S,}
.  error
.else
.  error
.endif

all:
	@echo ok
