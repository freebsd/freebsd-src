# $NetBSD: varname-makeflags.mk,v 1.8 2023/06/01 07:27:30 rillig Exp $
#
# Tests for the environment variable 'MAKEFLAGS', from which additional
# command line arguments are read before the actual command line arguments.
#
# After reading the makefiles and before making the targets, the arguments
# that were collected in '.MAKEFLAGS' and '.MAKEOVERRIDES' are written back to
# the environment variable 'MAKEFLAGS'.

all: spaces_stage_0 dollars_stage_0 append_stage_0 override_stage_0


.if !make(*stage*)

# The unit tests are run with an almost empty environment.  In particular,
# the variable MAKEFLAGS is not set.
.  if ${MAKEFLAGS:Uundefined} != "undefined"
.    error
.  endif

# The special variable .MAKEFLAGS is influenced though.
# See varname-dot-makeflags.mk for more details.
.  if ${.MAKEFLAGS} != " -r -k"
.    error
.  endif


# In POSIX mode, the environment variable MAKEFLAGS can contain letters only,
# for compatibility.  These letters are exploded to form regular options.
OUTPUT!=	env MAKEFLAGS=ikrs ${MAKE} -f /dev/null -v .MAKEFLAGS
.  if ${OUTPUT} != " -i -k -r -s -V .MAKEFLAGS"
.    error
.  endif

# As soon as there is a single non-alphabetic character in the environment
# variable MAKEFLAGS, it is no longer split.  In this example, the word
# "d0ikrs" is treated as a target, but the option '-v' prevents any targets
# from being built.
OUTPUT!=	env MAKEFLAGS=d0ikrs ${MAKE} -r -f /dev/null -v .MAKEFLAGS
.  if ${OUTPUT} != " -r -V .MAKEFLAGS"
.    error ${OUTPUT}
.  endif

.endif


# When options are parsed, the option and its argument are appended as
# separate words to the MAKEFLAGS for the child processes.  Special characters
# in the option arguments are not quoted though.
spaces_stage_0:
	@echo '$@: MAKEFLAGS=<'${MAKEFLAGS:Q}'>'
	@echo "$@: env MAKEFLAGS=<$$MAKEFLAGS>"
	@${MAKE} -f ${MAKEFILE} spaces_stage_1 -d00000 -D"VARNAME WITH SPACES"

# At this point, the 'VARNAME WITH SPACES' is no longer recognizable as a
# single command line argument.  In practice, variable names don't contain
# spaces.
spaces_stage_1:
	@echo '$@: MAKEFLAGS=<'${MAKEFLAGS:Q}'>'
	@echo "$@: env MAKEFLAGS=<$$MAKEFLAGS>"


# Demonstrate that '$' characters are altered when they are passed on to child
# make processes via MAKEFLAGS.
dollars_stage_0:
	@echo '$@: MAKEFLAGS=<'${MAKEFLAGS:Q}'>'

	# The '$$$$' becomes a literal '$$' when building the '${MAKE}'
	# command line, making the actual argument 'DOLLARS=$${varname}'.
	# At this stage, MAKEFLAGS is not yet involved.
	@${MAKE} -f ${MAKEFILE} dollars_stage_1 DOLLARS='$$$${varname}'

.if make(dollars_stage_1)
# At this point, the variable 'DOLLARS' contains '$${varname}', which
# evaluates to a literal '$' followed by '{varname}'.
.  if ${DOLLARS} != "\${varname}"
.    error
.  endif
.endif
dollars_stage_1:
	# At this point, the stage 1 make provides the environment variable
	# 'MAKEFLAGS' to its child processes, even if the child process is not
	# another make.
	#
	# expect: dollars_stage_1: env MAKEFLAGS=< -r -k DOLLARS=\$\{varname\}>
	#
	# The 'DOLLARS=\$\{varname\}' assignment is escaped so that the stage
	# 2 make will see it as a single word.
	@echo "$@: env MAKEFLAGS=<$$MAKEFLAGS>"

	# At this point, evaluating the environment variable 'MAKEFLAGS' leads
	# to strange side effects as the string '\$\{varname\}' is interpreted
	# as:
	#
	#	\		a literal string of a single backslash
	#	$\		the value of the variable named '\'
	#	{varname\}	a literal string
	#
	# Since the variable named '\' is not defined, the resulting value is
	# '\{varname\}'.  Make doesn't handle isolated '$' characters in
	# strings well, instead each '$' has to be part of a '$$' or be part
	# of a subexpression like '${VAR}'.
	@echo '$@: MAKEFLAGS=<'${MAKEFLAGS:Q}'>'

	# The modifier ':q' preserves a '$$' in an expression value instead of
	# expanding it to a single '$', but it's already too late, as that
	# modifier applies after the expression has been evaluated.  Except
	# for debug logging, there is no way to process strings that contain
	# isolated '$'.
	@echo '$@: MAKEFLAGS:q=<'${MAKEFLAGS:q}'>'

	@${MAKE} -f ${MAKEFILE} dollars_stage_2

.if make(dollars_stage_2)
# At this point, the variable 'DOLLARS' contains '${varname}', and since
# 'varname' is undefined, that expression evaluates to an empty string.
.  if ${DOLLARS} != ""
.    error
.  endif
varname=	varvalue
.  if ${DOLLARS} != "varvalue"
.    error
.  endif
.  undef varname
.endif
dollars_stage_2:
	@echo "$@: env MAKEFLAGS=<$$MAKEFLAGS>"
	@echo '$@: dollars=<'${DOLLARS:Q}'>'
	@echo '$@: MAKEFLAGS=<'${MAKEFLAGS:Q}'>'
	@${MAKE} -f ${MAKEFILE} dollars_stage_3

dollars_stage_3:
	@echo "$@: env MAKEFLAGS=<$$MAKEFLAGS>"
	@echo '$@: dollars=<'${DOLLARS:Uundefined:Q}'>'
	@echo '$@: MAKEFLAGS=<'${MAKEFLAGS:Q}'>'


# Demonstrates in which exact order the MAKEFLAGS are built from the parent
# MAKEFLAGS and the flags from the command line, in particular that variable
# assignments are passed at the end, after the options.
append_stage_0:
	@echo '$@: MAKEFLAGS=<'${MAKEFLAGS:Q}'>'
	@${MAKE} -Dbefore-0 -f ${MAKEFILE} append_stage_1 VAR0=value -Dafter-0

append_stage_1:
	@echo '$@: MAKEFLAGS=<'${MAKEFLAGS:Q}'>'
	@${MAKE} -Dbefore-1 -f ${MAKEFILE} append_stage_2 VAR1=value -Dafter-1

append_stage_2:
	@echo '$@: MAKEFLAGS=<'${MAKEFLAGS:Q}'>'
	@${MAKE} -Dbefore-2 -f ${MAKEFILE} append_stage_3 VAR2=value -Dafter-2

append_stage_3:
	@echo '$@: MAKEFLAGS=<'${MAKEFLAGS:Q}'>'


# Demonstrates the implementation details of 'MAKEFLAGS', in particular that
# it is an environment variable rather than a global variable.
override_stage_0:
	@${MAKE} -f ${MAKEFILE} STAGE=1 VAR=value override_stage_1

.if make(override_stage_1)
# While parsing the makefiles, 'MAKEFLAGS' is the value of the environment
# variable, in this case provided by stage 0.
.  if ${MAKEFLAGS:M*} != "-r -k"
.    error
.  endif
MAKEFLAGS=	overridden	# temporarily override it
.  if ${MAKEFLAGS} != "overridden"
.    error
.  endif
.undef MAKEFLAGS		# make the environment variable visible again
.  if ${MAKEFLAGS:M*} != "-r -k"
.    error
.  endif
.endif
override_stage_1:
	@echo '$@: run MAKEFLAGS=<'${MAKEFLAGS:Q}'>'
	@${MAKE} -f ${MAKEFILE} STAGE=2 override_stage_2

override_stage_2:
	@echo '$@: STAGE=<${STAGE}> VAR=<${VAR}>'
