# $NetBSD: cond-func-make.mk,v 1.3 2020/09/25 20:11:06 rillig Exp $
#
# Tests for the make() function in .if conditions, which tests whether
# the argument has been passed as a target via the command line or later
# via the .MAKEFLAGS special dependency target.

.if !make(via-cmdline)
.  error
.endif
.if make(via-dot-makeflags)
.  error
.endif

.MAKEFLAGS: via-dot-makeflags

.if !make(via-cmdline)
.  error
.endif
.if !make(via-dot-makeflags)
.  error
.endif

via-cmdline via-dot-makeflags:
	: $@
