# $NetBSD: directive-endif.mk,v 1.5 2020/12/14 21:56:17 rillig Exp $
#
# Tests for the .endif directive.
#
# Since 2020-12-15, the .endif directive no longer accepts arguments.
# The manual page had never allowed that, but the code didn't check it.
#
# See also:
#	Cond_EvalLine

# TODO: Implementation

.MAKEFLAGS: -dL

# Error: .endif does not take arguments
.if 0
# Since 2020-12-15, complain about the extra text after the 'endif'.
.endif 0

# Error: .endif does not take arguments
.if 1
# Since 2020-12-15, complain about the extra text after the 'endif'.
.endif 1

# Comments are allowed after an '.endif'.
.if 2
.endif # comment

# Only whitespace and comments are allowed after an '.endif', but nothing
# else.
.if 1
# Since 2020-12-15, complain about the extra text after the 'endif'.
.endif0

# Only whitespace and comments are allowed after an '.endif', but nothing
# else.
.if 1
# Since 2020-12-15, complain about the extra text after the 'endif'.
.endif/

# After an '.endif', no other letter must occur.  This 'endifx' is not
# parsed as an 'endif', therefore another '.endif' must follow to balance
# the directives.
.if 1
.endifx
.endif # to close the preceding '.if'

all:
	@:;
