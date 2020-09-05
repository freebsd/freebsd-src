# $NetBSD: cond-func-commands.mk,v 1.3 2020/08/23 14:07:20 rillig Exp $
#
# Tests for the commands() function in .if conditions.

.MAIN: all

# The target "target" does not exist yet, therefore it cannot have commands.
.if commands(target)
.error
.endif

target:

# Now the target exists, but it still has no commands.
.if commands(target)
.error
.endif

target:
	# not a command

# Even after the comment, the target still has no commands.
.if commands(target)
.error
.endif

target:
	@:;

# Finally the target has commands.
.if !commands(target)
.error
.endif

all:
	@:;
