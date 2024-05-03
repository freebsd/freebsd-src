# $NetBSD: cmd-errors.mk,v 1.6 2024/04/23 22:51:28 rillig Exp $
#
# Demonstrate how errors in expressions affect whether the commands
# are actually executed in compat mode.

all: undefined unclosed-expression unclosed-modifier unknown-modifier end

# Undefined variables in expressions are not an error.  They expand to empty
# strings.
undefined:
	: $@-${UNDEFINED}-eol

# XXX: As of 2020-11-01, this command is executed even though it contains
# parse errors.
unclosed-expression:
	: $@-${UNCLOSED

# XXX: As of 2020-11-01, this command is executed even though it contains
# parse errors.
unclosed-modifier:
	: $@-${UNCLOSED:

# XXX: As of 2020-11-01, this command is executed even though it contains
# parse errors.
unknown-modifier:
	: $@-${UNKNOWN:Z}-eol

end:
	: $@-eol

# XXX: As of 2020-11-02, despite the parse errors, the exit status is 0.
