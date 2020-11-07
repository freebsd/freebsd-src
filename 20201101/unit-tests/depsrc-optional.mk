# $NetBSD: depsrc-optional.mk,v 1.3 2020/09/05 15:57:12 rillig Exp $
#
# Tests for the special source .OPTIONAL in dependency declarations,
# which ignores the target if make cannot find out how to create it.
#
# TODO: Describe practical use cases for this feature.

# TODO: Explain why the commands for "important" are not executed.
# I had thought that only the "optional" commands were skipped.

all: important
	: ${.TARGET} is made.

important: optional
	: ${.TARGET} is made.

optional: .OPTIONAL
	: This is not executed.
