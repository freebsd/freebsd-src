# $NetBSD: deptgt-ignore.mk,v 1.4 2022/01/22 21:50:41 rillig Exp $
#
# Tests for the special target .IGNORE in dependency declarations, which
# does not stop if a command from this target exits with a non-zero status.
#
# This test only applies to compatibility mode.  In jobs mode such as with
# '-j1', all commands for a single target are bundled into a single shell
# program, which is a different implementation technique, the .IGNORE applies
# there as well.

.MAKEFLAGS: -d0			# force stdout to be unbuffered

all: depends-on-failed depends-on-ignored
.PHONY: all depends-on-failed depends-on-ignored error-failed error-ignored

error-failed error-ignored:
	@echo '${.TARGET} before'
	@false
	@echo '${.TARGET} after'

depends-on-failed: error-failed
	@echo 'Making ${.TARGET} from ${.ALLSRC}.'
depends-on-ignored: error-ignored
	@echo 'Making ${.TARGET} from ${.ALLSRC}.'

# Even though the command 'false' in the middle fails, the remaining commands
# are still run.  After that, the target is marked made, so targets depending
# on the target with the ignored commands are made.
.IGNORE: error-ignored

#.MAKEFLAGS: -dg2
