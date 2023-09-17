# $NetBSD: directive-endif.mk,v 1.7 2023/06/01 20:56:35 rillig Exp $
#
# Tests for the .endif directive.
#
# Since 2020-12-15, the .endif directive no longer accepts arguments.
# The manual page had never allowed that, but the code didn't check it.
#
# See also:
#	Cond_EvalLine

.MAKEFLAGS: -dL

.if 0
# Since 2020-12-15:
# expect+1: The .endif directive does not take arguments
.endif 0

.if 1
# Since 2020-12-15:
# expect+1: The .endif directive does not take arguments
.endif 1

# Comments are allowed after an '.endif'.
.if 2
.endif # comment

# Only whitespace and comments are allowed after an '.endif', but nothing
# else.
.if 1
# Since 2020-12-15:
# expect+1: The .endif directive does not take arguments
.endif0

# Only whitespace and comments are allowed after an '.endif', but nothing
# else.
.if 1
# Since 2020-12-15:
# expect+1: The .endif directive does not take arguments
.endif/

# After an '.endif', no other letter must occur.
.if 1
# expect+1: Unknown directive "endifx"
.endifx
.endif				# to close the preceding '.if'
