# $NetBSD: directive-export-literal.mk,v 1.8 2024/06/01 18:44:05 rillig Exp $
#
# Tests for the .export-literal directive, which exports a variable value
# without expanding it.

UT_VAR=		value with ${UNEXPANDED} expression

.export-literal UT_VAR

.export-literal			# oops: missing argument

# After a variable whose value does not contain a '$' is exported, a following
# .export-literal can be skipped, to avoid a setenv call, which may leak
# memory on some platforms.
UT_TWICE_LITERAL=	value literal
.export UT_TWICE_LITERAL
.export-literal UT_TWICE_LITERAL

# XXX: After an .export, an .export-literal has no effect, even when the
# variable value contains a '$'.
UT_TWICE_EXPR=		value ${indirect:L}
.export UT_TWICE_EXPR
.export-literal UT_TWICE_EXPR

# After an .export, an .unexport resets the variable's exported state,
# re-enabling a later .export-literal.
UT_TWICE_EXPR_UNEXPORT=	value ${indirect:L}
.export UT_TWICE_EXPR_UNEXPORT
.unexport UT_TWICE_EXPR_UNEXPORT
.export-literal UT_TWICE_EXPR_UNEXPORT

all:
	@echo "$$UT_VAR"
	@echo "$$UT_TWICE_LITERAL"
	@echo "$$UT_TWICE_EXPR"
	@echo "$$UT_TWICE_EXPR_UNEXPORT"
