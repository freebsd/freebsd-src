# $NetBSD: directive-export-env.mk,v 1.3 2020/11/03 17:17:31 rillig Exp $
#
# Tests for the .export-env directive.

# TODO: Implementation

.export-en			# oops: misspelled
.export-env
.export-environment		# oops: misspelled

all:
	@:;
