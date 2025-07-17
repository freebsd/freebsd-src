# $NetBSD: opt-query.mk,v 1.7 2022/08/18 05:37:05 rillig Exp $
#
# Tests for the -q command line option.
#
# The -q option only looks at the dependencies between the targets.
# None of the commands in the targets are run, not even those that are
# prefixed with '+'.

# This test consists of several parts:
#
#	main		Delegates to the actual tests.
#
#	commands	Ensures that none of the targets is made.
#
#	variants	Ensures that the up-to-date status is correctly
#			reported in both compat and jobs mode, and for several
#			kinds of make targets.
PART?=	main

.if ${PART} == "main"

all: .PHONY variants cleanup

_!=	touch -f opt-query-file.up-to-date

variants: .PHONY

.  for target in commands
	@echo 'Making ${target}':
	@${MAKE} -r -f ${MAKEFILE} -q ${mode:Mjobs:%=-j1} ${target} PART=commands \
	&& echo "${target}: query status $$?" \
	|| echo "${target}: query status $$?"
	@echo
.  endfor

.  for mode in compat jobs
.    for target in opt-query-file.out-of-date opt-query-file.up-to-date phony
	@echo 'Making ${target} in ${mode} mode':
	@${MAKE} -r -f ${MAKEFILE} -q ${mode:Mjobs:%=-j1} ${target} PART=variants \
	&& echo "${target} in ${mode} mode: query status $$?" \
	|| echo "${target} in ${mode} mode: query status $$?"
	@echo
.    endfor
.  endfor

# Between 1994 and before 2022-08-17, the exit status for '-q' was always 1,
# the cause for that exit code varied over time though.
#
# expect: opt-query-file.out-of-date in compat mode: query status 1
# expect: opt-query-file.up-to-date in compat mode: query status 0
# expect: phony in compat mode: query status 1
# expect: opt-query-file.out-of-date in jobs mode: query status 1
# expect: opt-query-file.up-to-date in jobs mode: query status 0
# expect: phony in jobs mode: query status 1

cleanup: .PHONY
	@rm -f opt-query-file.up-to-date

.elif ${PART} == "commands"

# This command cannot be prevented from being run since it is used at parse
# time, and any later variable assignments may depend on its result.
!=	echo 'command during parsing' 1>&2; echo

# None of these commands are run.
.BEGIN:
	@echo '$@: hidden command'
	@+echo '$@: run always'

# None of these commands are run.
commands:
	@echo '$@: hidden command'
	@+echo '$@: run always'
# The exit status 1 is because the "commands" target has to be made, that is,
# it is not up-to-date.

.elif ${PART} == "variants"

opt-query-file.out-of-date: ${MAKEFILE}
opt-query-file.up-to-date: ${MAKEFILE}
phony: .PHONY

.else
.  error Invalid part '${PART}'
.endif
