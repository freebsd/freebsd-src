# $NetBSD: cmd-errors.mk,v 1.13 2025/06/28 22:39:28 rillig Exp $
#
# Demonstrate how errors in expressions affect whether the commands
# are actually executed in compat mode.

all: undefined unclosed-expression unclosed-modifier unknown-modifier end

# Undefined variables in expressions are not an error.  They expand to empty
# strings.
undefined:
# expect: : undefined--eol
	: $@-${UNDEFINED}-eol

unclosed-expression:
# expect: make: Unclosed variable "UNCLOSED"
# expect-not: : unclosed-expression-
	: $@-${UNCLOSED

unclosed-modifier:
# expect: make: Unclosed expression, expecting "}"
# expect-not: : unclosed-modifier-
	: $@-${UNCLOSED:

unknown-modifier:
# expect: make: Unknown modifier ":Z"
# expect-not: : unknown-modifier--eol
	: $@-${UNKNOWN:Z}-eol

end:
# expect: : end-eol
	: $@-eol

# expect: exit status 2
