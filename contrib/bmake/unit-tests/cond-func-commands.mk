# $NetBSD: cond-func-commands.mk,v 1.5 2020/11/15 14:07:53 rillig Exp $
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

all:
	@:;
