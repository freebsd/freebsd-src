# $NetBSD: directive-export-literal.mk,v 1.6 2020/11/03 17:17:31 rillig Exp $
#
# Tests for the .export-literal directive, which exports a variable value
# without expanding it.

UT_VAR=		value with ${UNEXPANDED} expression

.export-literal UT_VAR

.export-litera			# oops: misspelled
.export-literal			# oops: missing argument
.export-literally		# oops: misspelled

all:
	@echo "$$UT_VAR"
