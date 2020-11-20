# $NetBSD: directive-unexport-env.mk,v 1.3 2020/11/03 17:17:31 rillig Exp $
#
# Tests for the .unexport-env directive.

# TODO: Implementation

.unexport-en			# oops: misspelled
.unexport-env			# ok
.unexport-environment		# oops: misspelled

all:
	@:;
