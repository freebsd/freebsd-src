# $NetBSD: cmd-errors-lint.mk,v 1.1 2020/11/02 20:43:27 rillig Exp $
#
# Demonstrate how errors in variable expansions affect whether the commands
# are actually executed.

.MAKEFLAGS: -dL

all: undefined unclosed-variable unclosed-modifier unknown-modifier end

# Undefined variables are not an error.  They expand to empty strings.
undefined:
	: $@ ${UNDEFINED}

# XXX: As of 2020-11-01, this obvious syntax error is not detected.
# XXX: As of 2020-11-01, this command is executed even though it contains
# parse errors.
unclosed-variable:
	: $@ ${UNCLOSED

# XXX: As of 2020-11-01, this obvious syntax error is not detected.
# XXX: As of 2020-11-01, this command is executed even though it contains
# parse errors.
unclosed-modifier:
	: $@ ${UNCLOSED:

# XXX: As of 2020-11-01, this command is executed even though it contains
# parse errors.
unknown-modifier:
	: $@ ${UNKNOWN:Z}

end:
	: $@
