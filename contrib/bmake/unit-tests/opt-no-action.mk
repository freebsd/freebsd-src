# $NetBSD: opt-no-action.mk,v 1.4 2020/11/09 20:50:56 rillig Exp $
#
# Tests for the -n command line option, which runs almost no commands.
# It just outputs them, to be inspected by human readers.
# Only commands that are in a .MAKE target or prefixed by '+' are run.

.MAKEFLAGS: -n

# This command cannot be prevented from being run since it is used at parse
# time, and any later variable assignments may depend on its result.
!=	echo 'command during parsing' 1>&2; echo

all: main
all: run-always

# Both of these commands are printed, but only the '+' command is run.
.BEGIN:
	@echo '$@: hidden command'
	@+echo '$@: run always'

# Both of these commands are printed, but only the '+' command is run.
main:
	@echo '$@: hidden command'
	@+echo '$@: run always'

# None of these commands is printed, but both are run, because this target
# depends on the special source ".MAKE".
run-always: .MAKE
	@echo '$@: hidden command'
	@+echo '$@: run always'

# Both of these commands are printed, but only the '+' command is run.
.END:
	@echo '$@: hidden command'
	@+echo '$@: run always'
