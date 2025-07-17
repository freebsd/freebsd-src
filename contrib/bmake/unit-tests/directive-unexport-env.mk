# $NetBSD: directive-unexport-env.mk,v 1.9 2023/06/01 20:56:35 rillig Exp $
#
# Tests for the .unexport-env directive.
#
# Before 2020-12-13, the directive unexport-env wrongly accepted arguments
# and ignored them.
#
# Before 2020-12-13, misspelled directive names like "unexport-environment"
# were not properly detected.

# TODO: Implementation

# expect+1: Unknown directive "unexport-en"
.unexport-en			# misspelled
.unexport-env			# ok
# expect+1: Unknown directive "unexport-environment"
.unexport-environment		# misspelled

.MAKEFLAGS: -dv
UT_EXPORTED=	value
UT_UNEXPORTED=	value
.export UT_EXPORTED
# expect+1: The directive .unexport-env does not take arguments
.unexport-env UT_EXPORTED UT_UNEXPORTED
.MAKEFLAGS: -d0

all: .PHONY
