# $NetBSD: posix-execution.mk,v 1.1 2025/04/13 09:29:32 rillig Exp $
#
# https://pubs.opengroup.org/onlinepubs/9799919799/utilities/make.html#tag_20_76_13_03
#

.POSIX:


# The target consists of two commands, which are executed separately.
# The second command thus does not see the shell variable from the first
# command.
# expect: one-at-a-time: shell_variable is 'second'
all: one-at-a-time
one-at-a-time:
	@shell_variable=first
	@echo "$@: shell_variable is '$${shell_variable:-second}'"


# expect: echo "prefixes: ignore errors"; exit 13
# expect: prefixes: ignore errors
# expect-not: echo "prefixes: no echo"
# expect: prefixes: no echo
# expect: prefixes: always, no echo
all: prefixes
prefixes:
	-echo "$@: ignore errors"; exit 13
	@echo "$@: no echo"
	+@echo "$@: always, no echo"


# Deviation from POSIX: The shell "-e" option is not in effect.
# expect: shell-e-option: before
# expect: shell-e-option: after
all: shell-e-option
shell-e-option:
	@echo '$@: before'; false; echo '$@: after'


# expect-not-matches: ^do%-prefix%-plus: a regular command
# expect: do-prefix-plus: prefixed by plus
# expect: do-prefix-plus: prefixed by plus
all: prefix-plus
prefix-plus:
	@${MAKE} -f ${MAKEFILE} -n do-prefix-plus
	@${MAKE} -f ${MAKEFILE} -n -j1 do-prefix-plus
do-prefix-plus:
	@echo '$@: a regular command'
	@+echo '$@: prefixed by plus'
	@echo '$@: a regular command'


# expect: do-error-not-ignored: successful
# expect-not: do-error-not-ignored: after an error
all: error-not-ignored
error-not-ignored:
	@${MAKE} -f ${MAKEFILE} do-error-not-ignored || :
do-error-not-ignored:
	@echo '$@: successful'; exit 13
	@echo '$@: after an error'
