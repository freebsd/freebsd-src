# $NetBSD: varname-makeflags.mk,v 1.3 2020/12/01 20:37:30 rillig Exp $
#
# Tests for the special MAKEFLAGS variable, which is basically just a normal
# environment variable.  It is closely related to .MAKEFLAGS but captures the
# state of .MAKEFLAGS at the very beginning of make, before any makefiles are
# read.

# TODO: Implementation

.MAKEFLAGS: -d0

# The unit tests are run with an almost empty environment.  In particular,
# the variable MAKEFLAGS is not set.  The '.MAKEFLAGS:' above also doesn't
# influence the environment variable MAKEFLAGS, therefore it is still
# undefined at this point.
.if ${MAKEFLAGS:Uundefined} != "undefined"
.  error
.endif

# The special variable .MAKEFLAGS is influenced though.
# See varname-dot-makeflags.mk for more details.
.if ${.MAKEFLAGS} != " -r -k -d 0"
.  error
.endif

all:
