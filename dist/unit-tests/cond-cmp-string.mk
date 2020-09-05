# $NetBSD: cond-cmp-string.mk,v 1.3 2020/08/20 18:43:19 rillig Exp $
#
# Tests for string comparisons in .if conditions.

# This is a simple comparison of string literals.
# Nothing surprising here.
.if "str" != "str"
.error
.endif

# The right-hand side of the comparison may be written without quotes.
.if "str" != str
.error
.endif

# The left-hand side of the comparison must be enclosed in quotes.
# This one is not enclosed in quotes and thus generates an error message.
.if str != str
.error
.endif

# The left-hand side of the comparison requires a defined variable.
# The variable named "" is not defined, but applying the :U modifier to it
# makes it "kind of defined" (see VAR_KEEP).  Therefore it is ok here.
.if ${:Ustr} != "str"
.error
.endif

# Any character in a string literal may be escaped using a backslash.
# This means that "\n" does not mean a newline but a simple "n".
.if "string" != "\s\t\r\i\n\g"
.error
.endif

# It is not possible to concatenate two string literals to form a single
# string.
.if "string" != "str""ing"
.error
.endif
