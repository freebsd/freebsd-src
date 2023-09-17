# $NetBSD: depsrc-optional.mk,v 1.5 2020/11/08 10:33:47 rillig Exp $
#
# Tests for the special source .OPTIONAL in dependency declarations,
# which ignores the target if make cannot find out how to create it.
#
# TODO: Describe practical use cases for this feature.

all: important
	: ${.TARGET} is made.

important: optional optional-cohort
	: ${.TARGET} is made.

optional: .OPTIONAL
	: An optional leaf node is not executed.

# See IsOODateRegular.
optional-cohort:: .OPTIONAL
	: A leaf node using '::' is considered out-of-date.

.MAKEFLAGS: -dm
