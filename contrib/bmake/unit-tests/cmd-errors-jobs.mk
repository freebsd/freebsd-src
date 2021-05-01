# $NetBSD: cmd-errors-jobs.mk,v 1.1 2020/12/27 05:11:40 rillig Exp $
#
# Demonstrate how errors in variable expansions affect whether the commands
# are actually executed in jobs mode.

.MAKEFLAGS: -j1

all: undefined unclosed-variable unclosed-modifier unknown-modifier end

# Undefined variables are not an error.  They expand to empty strings.
undefined:
	: $@ ${UNDEFINED} eol

# XXX: As of 2020-11-01, this command is executed even though it contains
# parse errors.
unclosed-variable:
	: $@ ${UNCLOSED

# XXX: As of 2020-11-01, this command is executed even though it contains
# parse errors.
unclosed-modifier:
	: $@ ${UNCLOSED:

# XXX: As of 2020-11-01, this command is executed even though it contains
# parse errors.
unknown-modifier:
	: $@ ${UNKNOWN:Z} eol

end:
	: $@ eol

# XXX: As of 2020-11-02, despite the parse errors, the exit status is 0.
