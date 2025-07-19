# $NetBSD: varmod.mk,v 1.30 2025/06/29 11:27:21 rillig Exp $
#
# Tests for variable modifiers, such as :Q, :S,from,to or :Ufallback.
#
# See also:
#	varparse-errors.mk

# As of 2024-06-05, the possible behaviors during parsing are:
#
# * `strict`: the parsing style used by most modifiers:
#   * either uses `ParseModifierPart` or parses the modifier literal
#   * other modifiers may follow, separated by a ':'
#
# * `greedy`: calls `ParseModifierPart` with `ch->endc`; this means
#   that no further modifiers are parsed in that expression.
#
# * `no-colon`: after parsing this modifier, the following modifier
#   does not need to be separated by a colon.
#   Omitting this colon is bad style.
#
# * `individual`: parsing this modifier does not follow the common
#   pattern of calling `ParseModifierPart`.
#
# The SysV column says whether a modifier falls back trying the `:from=to`
# System V modifier. Remarks:
#
#	In the assignment modifiers `::=` and its variants, the `=` is part of
#	the modifier name, so they never fall back to the `:from=to` modifier.
#
#	All no-colon modifiers get a "no", as the modifier name would be
#	trimmed off before the `:from=to` modifier could see them, for
#	example, ${VAR:LAR=ALUE} and ${VAR:L:AR=ALUE} behave the same.
#
# | **Modifier** | **Behavior** | **Remarks**        | **SysV** |
# |--------------|--------------|--------------------|----------|
# | !            | no-colon     |                    | no       |
# | :=           | greedy       |                    | no       |
# | :?=          | greedy       |                    | no       |
# | :+=          | greedy       |                    | no       |
# | :!=          | greedy       |                    | no       |
# | ?:           | greedy       |                    | no       |
# | @            | no-colon     |                    | no       |
# | C            | no-colon     |                    | no       |
# | D            | individual   | custom parser      | no       |
# | E            | strict       |                    | yes      |
# | H            | strict       |                    | yes      |
# | L            | no-colon     |                    | no       |
# | M            | individual   | custom parser      | no       |
# | N            | individual   | custom parser      | no       |
# | O            | strict       | only literal value | yes      |
# | P            | no-colon     |                    | no       |
# | Q            | strict       |                    | yes      |
# | R            | strict       |                    | yes      |
# | S            | no-colon     |                    | no       |
# | T            | strict       |                    | yes      |
# | U            | individual   | custom parser      | no       |
# | [            | strict       |                    | no       |
# | _            | individual   | strcspn            | no       |
# | gmtime       | strict       |                    | no       |
# | hash         | strict       |                    | yes      |
# | localtime    | strict       |                    | no       |
# | q            | strict       |                    | yes      |
# | range        | strict       |                    | no       |
# | sh           | strict       |                    | yes      |
# | t            | strict       |                    | yes      |
# | u            | strict       |                    | yes      |
# | from=to      | greedy       | SysV, fallback     | ---      |

# These tests assume
.MAKE.SAVE_DOLLARS = yes

DOLLAR1=	$$
DOLLAR2=	${:U\$}

# To get a single '$' sign in the value of an expression, it has to
# be written as '$$' in a literal variable value.
#
# See Var_Parse, where it calls Var_Subst.
.if ${DOLLAR1} != "\$"
.  error
.endif

# Another way to get a single '$' sign is to use the :U modifier.  In the
# argument of that modifier, a '$' is escaped using the backslash instead.
#
# See Var_Parse, where it calls Var_Subst.
.if ${DOLLAR2} != "\$"
.  error
.endif

# It is also possible to use the :U modifier directly in the expression.
#
# See Var_Parse, where it calls Var_Subst.
.if ${:U\$} != "\$"
.  error
.endif

# XXX: As of 2020-09-13, it is not possible to use '$$' in a variable name
# to mean a single '$'.  This contradicts the manual page, which says that
# '$' can be escaped as '$$'.
.if ${$$:L} != ""
.  error
.endif

# In lint mode, make prints helpful error messages.
# For compatibility, make does not print these error messages in normal mode.
# Should it?
.MAKEFLAGS: -dL
# expect+2: To escape a dollar, use \$, not $$, at "$$:L} != """
# expect+1: Invalid variable name ":", at "$:L} != """
.if ${$$:L} != ""
.  error
.endif

# A '$' followed by nothing is an error as well.
# expect+1: Dollar followed by nothing
.if ${:Uword:@word@${word}$@} != "word"
.  error
.endif

# The modifier :P does not fall back to the SysV modifier.
# Therefore the modifier :P=RE generates a parse error.
VAR=	STOP
# expect+1: Missing delimiter ":" after modifier "P"
.if ${VAR:P=RE} != "STORE"
.  error
.else
.  error
.endif

# Test the word selection modifier ':[n]' with a very large number that is
# larger than ULONG_MAX for any supported platform.
# expect+1: Invalid modifier ":[99333000222000111000]"
.if ${word:L:[99333000222000111000]}
.endif
# expect+1: Invalid modifier ":[2147483648]"
.if ${word:L:[2147483648]}
.endif

# Test the range generation modifier ':range=n' with a very large number that
# is larger than SIZE_MAX for any supported platform.
# expect+1: Invalid number "99333000222000111000}" for modifier ":range"
.if ${word:L:range=99333000222000111000}
.endif

# In an indirect modifier, the delimiter is '\0', which at the same time marks
# the end of the string.  The sequence '\\' '\0' is not an escaped delimiter,
# as it would be wrong to skip past the end of the string.
# expect+1: Invalid time value "\"
.if ${:${:Ugmtime=\\}}
.  error
.endif

# Test a '$' at the end of a modifier part, for all modifiers in the order
# listed in ApplyModifier.
#
# The only modifier parts where an unescaped '$' makes sense at the end are
# the 'from' parts of the ':S' and ':C' modifiers.  In all other modifier
# parts, an unescaped '$' is an undocumented and discouraged edge case, as it
# means the same as an escaped '$'.
.if ${:U:!printf '%s\n' $!} != "\$"
.  error
.endif
# expect+1: Dollar followed by nothing
.if ${VAR::=value$} != "" || ${VAR} != "value"
.  error
.endif
${:U }=		<space>
# expect+2: Dollar followed by nothing
# expect+1: Dollar followed by nothing
.if ${VAR::+=appended$} != "" || ${VAR} != "value<space>appended"
.  error
.endif
.if ${1:?then$:else$} != "then\$"
.  error
.endif
.if ${0:?then$:else$} != "else\$"
.  error
.endif
# expect+1: Dollar followed by nothing
.if ${word:L:@w@$w$@} != "word"
.  error
.endif
# expect+1: Invalid modifier ":[$]"
.if ${word:[$]}
.  error
.else
.  error
.endif
VAR_DOLLAR=	VAR$$
.if ${word:L:_=VAR$} != "word" || ${${VAR_DOLLAR}} != "word"
.  error
.endif
.if ${word:L:C,d$,m,} != "worm"
.  error
.endif
.if ${word:L:C,d,$,} != "wor\$"
.  error
.endif
# expect+2: Dollar followed by nothing
# expect+1: Invalid variable name "}", at "$} != "set""
.if ${VAR:Dset$} != "set"
.  error
.endif
# expect+1: Invalid variable name "}", at "$} != "fallback""
.if ${:Ufallback$} != "fallback"
.  error
.endif
# expect+1: Invalid time value "1000$"
.if ${%y:L:gmtime=1000$}
.  error
.else
.  error
.endif
# expect+1: Invalid time value "1000$"
.if ${%y:L:localtime=1000$}
.  error
.else
.  error
.endif
# expect+1: Dollar followed by nothing
.if ${word:L:Mw*$} != "word"
.  error
.endif
# expect+1: Dollar followed by nothing
.if ${word:L:NX*$} != "word"
.  error
.endif
# expect+1: Invalid argument "fallback$" for modifier ":mtime"
.if ${.:L:mtime=fallback$}
.  error
.else
.  error
.endif
.if ${word:L:S,d$,m,} != "worm"
.  error
.endif
.if ${word:L:S,d,m$,} != "worm\$"
.  error
.endif

.undef VAR
# expect+1: Missing delimiter ":" after modifier "L"
.if ${VAR:LAR=ALUE} != "VALUE"
.  error
.endif
.if ${VAR:L:AR=ALUE} != "VALUE"
.  error
.endif


# When an expression has the usual form ${...} with braces,
# in the part of a modifier, ":}\$" can be escaped using a backslash.
# All other characters are passed through unmodified.
# expect+1: Invalid time value " : } \ $ ) \) ( "
.if ${%Y:L:localtime= \: \} \\ \$ ) \) ( :M*} != ": } \\ \$ ) \\) ("
.  error
.endif
# When an expression has the unusual form $(...) with parentheses,
# in the part of a modifier, ":)\$" can be escaped using a backslash.
# All other characters are passed through unmodified.
# expect+1: Invalid time value " : \) \ $ "
.if ${%Y:L:localtime= \: \) \\ \$ } \} { :M*} != ": ) \\ \$ } \\} {"
.  error
.endif
# Same when the modifier is the last modifier in an expression.
# expect+1: Invalid time value " : } \ $ ) \) ( "
.if ${%Y:L:localtime= \: \} \\ \$ ) \) ( } != " : } \\ \$ ) \\) ( "
.  error
.endif
# Same when the modifier is the last modifier in an expression.
# expect+1: Invalid time value " : \) \ $ "
.if ${%Y:L:localtime= \: \) \\ \$ } \} { } != " : ) \\ \$ } \\} { "
.  error
.endif
