# $NetBSD: directive-unexport-env.mk,v 1.7 2020/12/12 18:11:42 rillig Exp $
#
# Tests for the .unexport-env directive.
#
# Before 2020-12-13, the directive unexport-env wrongly accepted arguments
# and ignored them.
#
# Before 2020-12-13, misspelled directive names like "unexport-environment"
# were not properly detected.

# TODO: Implementation

.unexport-en			# misspelled
.unexport-env			# ok
.unexport-environment		# misspelled

.MAKEFLAGS: -dv
UT_EXPORTED=	value
UT_UNEXPORTED=	value
.export UT_EXPORTED
.unexport-env UT_EXPORTED UT_UNEXPORTED
.MAKEFLAGS: -d0

all:
	@:;
