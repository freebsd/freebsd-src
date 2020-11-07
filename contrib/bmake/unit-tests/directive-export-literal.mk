# $NetBSD: directive-export-literal.mk,v 1.5 2020/10/05 19:27:48 rillig Exp $
#
# Tests for the .export-literal directive, which exports a variable value
# without expanding it.

UT_VAR=		value with ${UNEXPANDED} expression

.export-literal UT_VAR

all:
	@echo "$$UT_VAR"
