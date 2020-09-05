# $NetBSD: cond-func-target.mk,v 1.3 2020/08/23 14:07:20 rillig Exp $
#
# Tests for the target() function in .if conditions.

.MAIN: all

# The target "target" does not exist yet.
.if target(target)
.error
.endif

target:

# The target exists, even though it does not have any commands.
.if !target(target)
.error
.endif

target:
	# not a command

# Adding a comment to an existing target does not change whether the target
# is defined or not.
.if !target(target)
.error
.endif

target:
	@:;

# Adding a command to an existing target does not change whether the target
# is defined or not.
.if !target(target)
.error
.endif

all:
	@:;
