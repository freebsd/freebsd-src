# $NetBSD: cmdline-undefined.mk,v 1.4 2023/11/19 21:47:52 rillig Exp $
#
# Tests for undefined expressions in the command line.

all:
	# When the command line is parsed, variable assignments using the
	# '=' assignment operator do get their variable name expanded
	# (which probably occurs rarely in practice, if at all), but their
	# variable value is not expanded, as usual.
	#
	@echo 'The = assignment operator'
	@${.MAKE} -f ${MAKEFILE} print-undefined \
		CMDLINE='Undefined is $${UNDEFINED}.'
	@echo

	# The interesting case is using the ':=' assignment operator, which
	# expands its right-hand side.  But only those variables that are
	# defined.
	@echo 'The := assignment operator'
	@${.MAKE} -f ${MAKEFILE} print-undefined \
		CMDLINE:='Undefined is $${UNDEFINED}.'
	@echo

.if make(print-undefined)

.MAKEFLAGS: MAKEFLAGS_ASSIGN='Undefined is $${UNDEFINED}.'
.MAKEFLAGS: MAKEFLAGS_SUBST:='Undefined is $${UNDEFINED}.'

# expect+2: From the command line: Undefined is .
# expect+1: From the command line: Undefined is .
.info From the command line: ${CMDLINE}
# expect+2: From .MAKEFLAGS '=': Undefined is .
# expect+1: From .MAKEFLAGS '=': Undefined is .
.info From .MAKEFLAGS '=': ${MAKEFLAGS_ASSIGN}
# expect+2: From .MAKEFLAGS ':=': Undefined is .
# expect+1: From .MAKEFLAGS ':=': Undefined is .
.info From .MAKEFLAGS ':=': ${MAKEFLAGS_SUBST}

UNDEFINED?=	now defined

# expect+2: From the command line: Undefined is now defined.
# expect+1: From the command line: Undefined is now defined.
.info From the command line: ${CMDLINE}
# expect+2: From .MAKEFLAGS '=': Undefined is now defined.
# expect+1: From .MAKEFLAGS '=': Undefined is now defined.
.info From .MAKEFLAGS '=': ${MAKEFLAGS_ASSIGN}
# expect+2: From .MAKEFLAGS ':=': Undefined is now defined.
# expect+1: From .MAKEFLAGS ':=': Undefined is now defined.
.info From .MAKEFLAGS ':=': ${MAKEFLAGS_SUBST}

print-undefined:
.endif
