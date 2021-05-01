# $NetBSD: cond-token-number.mk,v 1.5 2020/11/15 14:58:14 rillig Exp $
#
# Tests for number tokens in .if conditions.
#
# TODO: Add introduction.

.if 0
.  error
.endif

# Even though -0 is a number and would be accepted by strtod, it is not
# accepted by the condition parser.
#
# See the ch_isdigit call in CondParser_String.
.if -0
.  error
.else
.  error
.endif

# Even though +0 is a number and would be accepted by strtod, it is not
# accepted by the condition parser.
#
# See the ch_isdigit call in CondParser_String.
.if +0
.  error
.else
.  error
.endif

# Even though -1 is a number and would be accepted by strtod, it is not
# accepted by the condition parser.
#
# See the ch_isdigit call in CondParser_String.
.if !-1
.  error
.else
.  error
.endif

# Even though +1 is a number and would be accepted by strtod, it is not
# accepted by the condition parser.
#
# See the ch_isdigit call in CondParser_String.
.if !+1
.  error
.else
.  error
.endif

# When the number comes from a variable expression though, it may be signed.
# XXX: This is inconsistent.
.if ${:U+0}
.  error
.endif

# When the number comes from a variable expression though, it may be signed.
# XXX: This is inconsistent.
.if !${:U+1}
.  error
.endif

# Hexadecimal numbers are accepted.
.if 0x0
.  error
.endif
.if 0x1
.else
.  error
.endif

# This is not a hexadecimal number, even though it has an x.
# It is interpreted as a string instead, effectively meaning defined(3x4).
.if 3x4
.else
.  error
.endif

# Ensure that parsing continues until here.
.info End of the tests.

all: # nothing
