# $NetBSD: varname-dot-make-level.mk,v 1.6 2025/03/22 12:23:00 rillig Exp $
#
# Tests for the special .MAKE.LEVEL variable, which informs about the
# recursion level.  It is related to the environment variable MAKELEVEL,
# even though they don't have the same value.

all: .PHONY level_1 set-env-same set-env-different

# expect: level 1: variable 0, env 1
level_1: .PHONY
	@printf 'level 1: variable %s, env %s\n' ${.MAKE.LEVEL} "$$${.MAKE.LEVEL.ENV}"
	@${MAKE} -f ${MAKEFILE} level_2

# expect: level 2: variable 1, env 2
level_2: .PHONY
	@printf 'level 2: variable %s, env %s\n' ${.MAKE.LEVEL} "$$${.MAKE.LEVEL.ENV}"
	@${MAKE} -f ${MAKEFILE} level_3

# The .unexport-env directive clears the environment, except for the
# .MAKE.LEVEL.ENV make variable, which by default refers to the MAKELEVEL
# environment variable.
.if make(level_2)
.unexport-env
.endif

# expect: level 3: variable 2, env 3
level_3: .PHONY
	@printf 'level 3: variable %s, env %s\n' ${.MAKE.LEVEL} "$$${.MAKE.LEVEL.ENV}"


# When a variable assignment from the command line tries to override a
# read-only global variable with the same value as before, ignore the
# assignment, as the variable value would not change.
#
# This special case allows older versions of make to coexist with newer
# versions of make. Older version of make (up to NetBSD 9) stored the internal
# .MAKE.LEVEL.ENV variable in the scope for command line variables, and these
# variables were passed to sub-makes via .MAKEOVERRIDES and the MAKEFLAGS
# environment variable. Newer versions of make (since NetBSD 11) store the
# internal .MAKE.LEVEL.ENV variable in the global scope but make it read-only
# and prevent any attempts to override it.
#
# https://gnats.netbsd.org/59184
set-env-same: .PHONY
	: ${.TARGET}
	@${MAKE} -f ${MAKEFILE} ok .MAKE.LEVEL.ENV=${.MAKE.LEVEL.ENV} || echo "${.TARGET}: exit $$?"


# expect: make: Cannot override read-only global variable ".MAKE.LEVEL.ENV" with a command line variable
set-env-different: .PHONY
	: ${.TARGET}
	@${MAKE} -f ${MAKEFILE} ok .MAKE.LEVEL.ENV=CUSTOM || echo "${.TARGET}: exit $$?"

ok: .PHONY
	@echo ok
