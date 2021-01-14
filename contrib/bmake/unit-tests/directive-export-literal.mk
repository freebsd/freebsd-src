# $NetBSD: directive-export-literal.mk,v 1.7 2020/12/13 01:07:54 rillig Exp $
#
# Tests for the .export-literal directive, which exports a variable value
# without expanding it.

UT_VAR=		value with ${UNEXPANDED} expression

.export-literal UT_VAR

.export-literal			# oops: missing argument

all:
	@echo "$$UT_VAR"
