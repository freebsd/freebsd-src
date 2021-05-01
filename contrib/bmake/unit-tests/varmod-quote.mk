# $NetBSD: varmod-quote.mk,v 1.3 2020/10/29 19:07:45 rillig Exp $
#
# Tests for the :Q variable modifier, which quotes the variable value
# to be used in a shell program.

# Any characters that might be interpreted by the shell are escaped.
# The set of escaped characters is the same, no matter which shell (sh, csh,
# ksh) is in effect.
.if ${:Ua b c:Q} != "a\\ b\\ c"
.  error
.endif

# The quote modifier only applies if the whole modifier name is "Q".
# There is no "Qshell" or "Qawk" or "Qregex" or even "Qhtml" variant.
# All strings except the plain "Q" are interpreted as SysV modifier.
.if ${:Ua.Qshell:Qshell=replaced} != "a.replaced"
.  error
.endif

all:
	@:;
