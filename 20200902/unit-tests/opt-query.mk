# $NetBSD: opt-query.mk,v 1.3 2020/08/19 05:13:18 rillig Exp $
#
# Tests for the -q command line option.
#
# The -q option only looks at the dependencies between the targets.
# None of the commands in the targets are run, not even those that are
# prefixed with '+'.

# This command cannot be prevented from being run since it is used at parse
# time, and any later variable assignments may depend on its result.
!=	echo 'command during parsing' 1>&2; echo

# None of these commands are run.
.BEGIN:
	@echo '$@: hidden command'
	@+echo '$@: run always'

# None of these commands are run.
all:
	@echo '$@: hidden command'
	@+echo '$@: run always'

# The exit status 1 is because the "all" target has to be made, that is,
# it is not up-to-date.
