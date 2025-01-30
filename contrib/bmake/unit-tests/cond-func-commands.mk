# $NetBSD: cond-func-commands.mk,v 1.6 2025/01/10 23:00:38 rillig Exp $
#
# Tests for the commands() function in .if conditions.

.MAIN: all

# At this point, the target 'target' does not exist yet, therefore it cannot
# have commands.  Sounds obvious, but good to know that it is really so.
.if commands(target)
.  error
.endif

target:

# Now the target exists, but it still has no commands.
.if commands(target)
.  error
.endif

target:
	# not a command

# Even after the comment, the target still has no commands.
.if commands(target)
.  error
.endif

target:
	@:;

# Finally the target has commands.
.if !commands(target)
.  error
.endif

# Expressions in the argument of a function call don't have to be defined.
.if commands(${UNDEF})
.  error
.endif

all:
	@:;
