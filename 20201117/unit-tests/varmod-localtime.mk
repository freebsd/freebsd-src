# $NetBSD: varmod-localtime.mk,v 1.5 2020/10/31 20:30:06 rillig Exp $
#
# Tests for the :localtime variable modifier, which formats a timestamp
# using strftime(3) in local time.

.if ${TZ} != "Europe/Berlin"	# see unit-tests/Makefile
.  error
.endif

all:	mod-localtime
all:	mod-localtime-indirect
all:	parse-errors

# Test for the default time format, %c.  Since the time always varies, it's
# only possible to check for the general format here.  The names of the
# month and weekday are always in English, independent from the locale.
# Example: Thu Oct 29 18:56:41 2020
.if ${:U:localtime:tW:M??? ??? ?? ??\:??\:?? ????} == ""
.  error
.endif

mod-localtime:
	@echo $@

	# modifier name too short
	@echo ${%Y:L:localtim=1593536400}

	# 2020-07-01T00:00:00Z
	@echo ${%Y:L:localtime=1593536400}

	# modifier name too long
	@echo ${%Y:L:localtimer=1593536400}

	# If the modifier name is not matched exactly, fall back to the
	# :from=to modifier.
	@echo ${localtime:L:local%=gm%} == gmtime

mod-localtime-indirect:
	@echo $@:

	# As of 2020-08-16, it is not possible to pass the seconds via a
	# variable expression.  This is because parsing of the :localtime
	# modifier stops at the '$' and returns to ApplyModifiers.
	#
	# There, a colon would be skipped but not a dollar.
	# Parsing therefore continues at the '$' of the ${:U159...}, looking
	# for an ordinary variable modifier.
	#
	# At this point, the ${:U} is expanded and interpreted as a variable
	# modifier, which results in the error message "Unknown modifier '1'".
	#
	# If ApplyModifier_Localtime were to pass its argument through
	# ParseModifierPart, this would work.
	@echo ${%Y:L:localtime=${:U1593536400}}

parse-errors:
	@echo $@:

	# As of 2020-10-31, it is possible to pass negative time stamps
	# to the :localtime modifier, resulting in dates before 1970.
	# Going back 50 years in the past is not a practical use case for
	# make.
	: -1 becomes ${:L:localtime=-1}.

	# Spaces are allowed, not because it would make sense but just as
	# a side-effect from using strtoul.
	: space 1 becomes ${:L:localtime= 1}.

	# 0 means now; to get consistent test results, the actual value has
	# to be normalized.
	: 0 becomes ${:L:localtime=0:C,^... ... .. ..:..:.. 20..$,ok,W}.

	: 1 becomes ${:L:localtime=1}.

	: INT32_MAX becomes ${:L:localtime=2147483647}.

	# This may be different if time_t is still a 32-bit signed integer.
	: INT32_MAX + 1 becomes ${:L:localtime=2147483648}.

	# Integer overflow.
	# Because this modifier is implemented using strtoul, the parsed
	# time is ULONG_MAX, which gets converted to -1.  This results
	# in a time stamp of the second before 1970 (in UTC) or 3599 seconds
	# after New Year's Day in Europe/Berlin.
	: overflow becomes ${:L:localtime=10000000000000000000000000000000}.

	# As of 2020-10-31, there is no error handling while parsing the
	# :localtime modifier, thus no error message is printed.  Parsing
	# stops after the '=', and the remaining string is parsed for
	# more variable modifiers.  Because of the unknown modifier 'e',
	# the whole variable value is discarded and thus not printed.
	: letter becomes ${:L:localtime=error}.
