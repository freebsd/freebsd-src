# $NetBSD: lint.mk,v 1.5 2023/11/19 21:47:52 rillig Exp $
#
# Demonstrates stricter checks that are only enabled in lint mode, using the
# option -dL.

# Before main.c 1.421 from 2020-11-01, make exited successfully even though
# the error message had been issued as PARSE_FATAL.  This was because back
# then, make checked for parse errors only after parsing each top-level
# makefile, in Parse_File.  After that, when expanding expressions
# in shell commands, the parse errors were not checked again.

# Ouch: as of 2020-08-03, the variable is malformed and parsing stops
# for a moment, but is continued after the wrongly-guessed end of the
# variable, which echoes "y@:Q}".

.MAKEFLAGS: -dL

all: mod-loop-varname

mod-loop-varname:
	@echo ${VAR:Uvalue:@${:Ubar:S,b,v,}@x${var}y@:Q}
	@echo ${VAR:Uvalue:@!@x$!y@:Q}	# surprisingly allowed
