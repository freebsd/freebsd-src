# $NetBSD: varmod.mk,v 1.9 2023/11/19 21:47:52 rillig Exp $
#
# Tests for variable modifiers, such as :Q, :S,from,to or :Ufallback.
#
# See also:
#	varparse-errors.mk

# As of 2022-08-06, the possible behaviors during parsing are:
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
# The SysV column says whether a parse error in the modifier falls back
# trying the `:from=to` System V modifier.
#
# | **Operator** | **Behavior** | **Remarks**        | **SysV** |
# |--------------|--------------|--------------------|----------|
# | `!`          | no-colon     |                    | no       |
# | `:=`         | greedy       |                    | yes      |
# | `?:`         | greedy       |                    | no       |
# | `@`          | no-colon     |                    | no       |
# | `C`          | no-colon     |                    | no       |
# | `D`          | individual   | custom parser      | N/A      |
# | `E`          | strict       |                    | yes      |
# | `H`          | strict       |                    | yes      |
# | `L`          | no-colon     |                    | N/A      |
# | `M`          | individual   | custom parser      | N/A      |
# | `N`          | individual   | custom parser      | N/A      |
# | `O`          | strict       | only literal value | no       |
# | `P`          | no-colon     |                    | N/A      |
# | `Q`          | strict       |                    | yes      |
# | `R`          | strict       |                    | yes      |
# | `S`          | no-colon     |                    | N/A      |
# | `T`          | strict       |                    | N/A      |
# | `U`          | individual   | custom parser      | N/A      |
# | `[`          | strict       |                    | no       |
# | `_`          | individual   | strcspn            | yes      |
# | `gmtime`     | strict       | only literal value | yes      |
# | `hash`       | strict       |                    | N/A      |
# | `localtime`  | strict       | only literal value | yes      |
# | `q`          | strict       |                    | yes      |
# | `range`      | strict       |                    | N/A      |
# | `sh`         | strict       |                    | N/A      |
# | `t`          | strict       |                    | no       |
# | `u`          | strict       |                    | yes      |
# | `from=to`    | greedy       | SysV, fallback     | N/A      |

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
# expect+1: Invalid variable name ':', at "$:L} != """
.if ${$$:L} != ""
.  error
.endif

# A '$' followed by nothing is an error as well.
# expect+1: Dollar followed by nothing
.if ${:Uword:@word@${word}$@} != "word"
.  error
.endif

# The variable modifier :P does not fall back to the SysV modifier.
# Therefore the modifier :P=RE generates a parse error.
# XXX: The .error should not be reached since the expression is
# malformed, and this error should be propagated up to Cond_EvalLine.
VAR=	STOP
# expect+1: Missing delimiter ':' after modifier "P"
.if ${VAR:P=RE} != "STORE"
# expect+1: Missing argument for ".error"
.  error
.endif

all: # nothing
