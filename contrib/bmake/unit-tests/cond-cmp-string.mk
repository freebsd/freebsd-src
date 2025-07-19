# $NetBSD: cond-cmp-string.mk,v 1.21 2025/06/28 22:39:28 rillig Exp $
#
# Tests for string comparisons in .if conditions.

# This is a simple comparison of string literals.
# Nothing surprising here.
.if "str" != "str"
.  error
.endif

# The right-hand side of the comparison may be written without quotes.
.if "str" != str
.  error
.endif

# The left-hand side of the comparison must be enclosed in quotes.
# This one is not enclosed in quotes and thus generates an error message.
# expect+1: Malformed conditional "str != str"
.if str != str
.  error
.endif

# An expression that occurs on the left-hand side of the comparison must be
# defined.
#
# The variable named "" is never defined, nevertheless it can be used as a
# starting point for an expression.  Applying the :U modifier to such an
# undefined expression turns it into a defined expression.
#
# See ApplyModifier_Defined and DEF_DEFINED.
.if ${:Ustr} != "str"
.  error
.endif

# Any character in a string literal may be escaped using a backslash.
# This means that "\n" does not mean a newline but a simple "n".
.if "string" != "\s\t\r\i\n\g"
.  error
.endif

# It is not possible to concatenate two string literals to form a single
# string.  In C, Python and the shell this is possible, but not in make.
# expect+1: Malformed conditional ""string" != "str""ing""
.if "string" != "str""ing"
.  error
.else
.  error
.endif

# There is no = operator for strings.
# expect+1: Malformed conditional "!("value" = "value")"
.if !("value" = "value")
.  error
.else
.  error
.endif

# There is no === operator for strings either.
# expect+1: Malformed conditional "!("value" === "value")"
.if !("value" === "value")
.  error
.else
.  error
.endif

# An expression can be enclosed in double quotes.
.if ${:Uword} != "${:Uword}"
.  error
.endif

# Between 2003-01-01 (maybe even earlier) and 2020-10-30, adding one of the
# characters " \t!=><" directly after an expression in a string literal
# resulted in a "Malformed conditional", even though the string was
# well-formed.
.if ${:Uword } != "${:Uword} "
.  error
.endif
# Some other characters worked though, and some didn't.
# Those that are mentioned in is_separator didn't work.
.if ${:Uword0} != "${:Uword}0"
.  error
.endif
.if ${:Uword&} != "${:Uword}&"
.  error
.endif
.if ${:Uword!} != "${:Uword}!"
.  error
.endif
.if ${:Uword<} != "${:Uword}<"
.  error
.endif

# Adding another expression to the string literal works though.
.if ${:Uword} != "${:Uwo}${:Urd}"
.  error
.endif

# Adding a space at the beginning of the quoted expression works though.
.if ${:U word } != " ${:Uword} "
.  error
.endif

# If at least one side of the comparison is a string literal, the string
# comparison is performed.
.if 12345 != "12345"
.  error
.endif

# If at least one side of the comparison is a string literal, the string
# comparison is performed.  The ".0" in the left-hand side makes the two
# sides of the equation unequal.
.if 12345.0 == "12345"
.  error
.endif

# Strings cannot be compared relationally, only for equality.
# expect+1: Comparison with "<" requires both operands "string" and "string" to be numeric
.if "string" < "string"
.  error
.else
.  error
.endif

# Strings cannot be compared relationally, only for equality.
# expect+1: Comparison with "<=" requires both operands "string" and "string" to be numeric
.if "string" <= "string"
.  error
.else
.  error
.endif

# Strings cannot be compared relationally, only for equality.
# expect+1: Comparison with ">" requires both operands "string" and "string" to be numeric
.if "string" > "string"
.  error
.else
.  error
.endif

# Strings cannot be compared relationally, only for equality.
# expect+1: Comparison with ">=" requires both operands "string" and "string" to be numeric
.if "string" >= "string"
.  error
.else
.  error
.endif

# Two expressions with different values compare unequal.
VAR1=	value1
VAR2=	value2
.if ${VAR1} != ${VAR2}
.else
.  error
.endif
