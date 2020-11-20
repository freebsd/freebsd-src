# $NetBSD: directive-endif.mk,v 1.3 2020/11/12 22:40:11 rillig Exp $
#
# Tests for the .endif directive.
#
# See also:
#	Cond_EvalLine

# TODO: Implementation

.MAKEFLAGS: -dL

# Error: .endif does not take arguments
# XXX: Missing error message
.if 0
.endif 0

# Error: .endif does not take arguments
# XXX: Missing error message
.if 1
.endif 1

# Comments are allowed after an '.endif'.
.if 2
.endif # comment

all:
	@:;
