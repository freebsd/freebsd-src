# $NetBSD: cond-func-make.mk,v 1.5 2023/06/23 04:56:54 rillig Exp $
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

# expect+1: warning: Unfinished character list in pattern argument '[' to function 'make'
.if make([)
.  error
.endif

via-cmdline via-dot-makeflags:
	: $@
