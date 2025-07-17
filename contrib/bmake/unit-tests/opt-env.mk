# $NetBSD: opt-env.mk,v 1.4 2022/03/26 13:32:31 rillig Exp $
#
# Tests for the -e command line option, which looks up environment variables
# before those from the global scope.  It has no influence on variables from
# the command line though.
#
# This option is required by POSIX.

# The variable FROM_ENV is defined in ./Makefile.

.MAKEFLAGS: -e

.if ${FROM_ENV} != value-from-env
.  error ${FROM_ENV}
.endif

# Try to override the variable; this does not have any effect.
FROM_ENV=	value-from-mk
.if ${FROM_ENV} != value-from-env
.  error ${FROM_ENV}
.endif

# Try to append to the variable; this also doesn't have any effect.
FROM_ENV+=	appended
.if ${FROM_ENV} != value-from-env
.  error ${FROM_ENV}
.endif

# The default assignment also cannot change the variable.
FROM_ENV?=	default
.if ${FROM_ENV} != value-from-env
.  error ${FROM_ENV}
.endif

# Neither can the assignment modifiers.
.if ${FROM_ENV::=from-condition}
.endif
.if ${FROM_ENV} != value-from-env
.  error ${FROM_ENV}
.endif

# Even .undef doesn't work since it only affects the global scope,
# which is independent from the environment variables.
.undef FROM_ENV
.if ${FROM_ENV} != value-from-env
.  error ${FROM_ENV}
.endif

all: .PHONY
