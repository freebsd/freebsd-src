# $NetBSD: lint.mk,v 1.2 2020/08/08 13:00:07 rillig Exp $
#
# Demonstrates stricter checks that are only enabled in the lint mode,
# using the -dL option.

# Ouch: as of 2020-08-03, make exits successfully even though the error
# message has been issued as PARSE_FATAL.

# Ouch: as of 2020-08-03, the variable is malformed and parsing stops
# for a moment, but is continued after the wrongly-guessed end of the
# variable, which echoes "y@:Q}".

all: mod-loop-varname

mod-loop-varname:
	@echo ${VAR:Uvalue:@${:Ubar:S,b,v,}@x${var}y@:Q}
	@echo ${VAR:Uvalue:@!@x$!y@:Q}	# surprisingly allowed
