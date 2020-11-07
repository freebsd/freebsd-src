# $NetBSD: cond-token-number.mk,v 1.3 2020/09/14 06:22:59 rillig Exp $
#
# Tests for number tokens in .if conditions.

.if 0
.  error
.endif

# Even though -0 is a number and would be accepted by strtod, it is not
# accepted by the condition parser.
#
# See the ch_isdigit call in CondParser_String.
.if -0
.  error
.endif

# Even though +0 is a number and would be accepted by strtod, it is not
# accepted by the condition parser.
#
# See the ch_isdigit call in CondParser_String.
.if +0
.  error
.endif

# Even though -1 is a number and would be accepted by strtod, it is not
# accepted by the condition parser.
#
# See the ch_isdigit call in CondParser_String.
.if !-1
.  error
.endif

# Even though +1 is a number and would be accepted by strtod, it is not
# accepted by the condition parser.
#
# See the ch_isdigit call in CondParser_String.
.if !+1
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

# Ensure that parsing continues until here.
.info End of the tests.

all: # nothing
