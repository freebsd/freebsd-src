# $NetBSD: cmd-errors-jobs.mk,v 1.8 2024/07/09 19:43:01 rillig Exp $
#
# Demonstrate how errors in expressions affect whether the commands
# are actually executed in jobs mode.

.MAKEFLAGS: -j1

all: undefined unclosed-expression unclosed-modifier unknown-modifier end

# Undefined variables in expressions are not an error.  They expand to empty
# strings.
# expect: : undefined--eol
undefined:
	: $@-${UNDEFINED}-eol

unclosed-expression:
# expect: make: in target "unclosed-expression": Unclosed variable "UNCLOSED"
# XXX: This command is executed even though it contains parse errors.
# expect: : unclosed-expression-
	: $@-${UNCLOSED

unclosed-modifier:
# expect: make: in target "unclosed-modifier": while evaluating variable "UNCLOSED" with value "": Unclosed expression, expecting '}'
# XXX: This command is executed even though it contains parse errors.
# expect: : unclosed-modifier-
	: $@-${UNCLOSED:

unknown-modifier:
# expect: make: in target "unknown-modifier": while evaluating variable "UNKNOWN" with value "": Unknown modifier "Z"
# XXX: This command is executed even though it contains parse errors.
# expect: : unknown-modifier--eol
	: $@-${UNKNOWN:Z}-eol

# expect: : end-eol
end:
	: $@-eol

# expect: exit status 2
