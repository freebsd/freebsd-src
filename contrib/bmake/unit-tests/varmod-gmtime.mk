# $NetBSD: varmod-gmtime.mk,v 1.21 2023/11/19 21:47:52 rillig Exp $
#
# Tests for the :gmtime variable modifier, which formats a timestamp
# using strftime(3) in UTC.
#
# See also:
#	varmod-localtime.mk

.if ${TZ:Uundefined} != "undefined"	# see unit-tests/Makefile
.  error
.endif

# Test for the default time format, %c.  Since the time always varies, it's
# only possible to check for the general format here.  The names of the
# month and weekday are always in English, independent from the locale.
# Example: Thu Oct 29 18:56:41 2020
.if ${:U:gmtime:tW:M??? ??? ?? ??\:??\:?? ????} == ""
.  error
.endif


# modifier name too short, falling back to the SysV modifier.
.if ${%Y:L:gmtim=1593536400} != "%Y"
.  error
.endif


# 2020-07-01T00:00:00Z
.if ${%Y:L:gmtime=1593536400} != "2020"
.  error
.endif


# modifier name too long, falling back to the SysV modifier.
.if ${%Y:L:gmtimer=1593536400} != "%Y"
.  error
.endif


# If the modifier name is not matched exactly, fall back to the
# :from=to modifier.
.if ${gmtime:L:gm%=local%} != "localtime"
.  error
.endif


# Before var.c 1.1050 from 2023-05-09, it was not possible to pass the
# seconds via an expression.
.if ${%Y:L:gmtime=${:U1593536400}} != "2020"
.  error
.endif


# Before var.c 1.631 from 2020-10-31 21:40:20, it was possible to pass
# negative time stamps to the :gmtime modifier, resulting in dates before
# 1970.  Going back 50 years in the past is not a practical use case for
# make.  Therefore, since var.c 1.631, negative time stamps produce a
# parse error.
# expect+2: Invalid time value "-1"
# expect+1: Malformed conditional (${:L:gmtime=-1} != "")
.if ${:L:gmtime=-1} != ""
.  error
.else
.  error
.endif


# Spaces were allowed before var.c 1.631 from 2020-10-31 21:40:20, not
# because it would make sense but just as a side-effect from using strtoul.
# expect+2: Invalid time value " 1"
# expect+1: Malformed conditional (${:L:gmtime= 1} != "")
.if ${:L:gmtime= 1} != ""
.  error
.else
.  error
.endif


# 0 means now; this differs from GNode.mtime, where a 0 means nonexistent.
# Since "now" constantly changes, the strongest possible test is to match the
# resulting pattern.
.if !${:L:gmtime=0:tW:M??? ??? ?? ??\:??\:?? 20??}
.  error
.endif


.if ${:L:gmtime=1} != "Thu Jan  1 00:00:01 1970"
.  error
.endif


# INT32_MAX
.if ${:L:gmtime=2147483647} != "Tue Jan 19 03:14:07 2038"
.  error
.endif


.if ${:L:gmtime=2147483648} == "Tue Jan 19 03:14:08 2038"
# All systems that have unsigned time_t or 64-bit time_t.
.elif ${:L:gmtime=2147483648} == "Fri Dec 13 20:45:52 1901"
# FreeBSD-12.0-i386 still has 32-bit signed time_t, see
# sys/x86/include/_types.h, __LP64__.
#
# Linux on 32-bit systems may still have 32-bit signed time_t, see
# sysdeps/unix/sysv/linux/generic/bits/typesizes.h, __TIMESIZE.
.else
.  error
.endif


# Integer overflow, at least before var.c 1.631 from 2020-10-31.
# Because this modifier is implemented using strtoul, the parsed time was
# ULONG_MAX, which got converted to -1.  This resulted in a time stamp of
# the second before 1970.
#
# Since var.c 1.631 from 2020-10-31, the overflow is detected and produces a
# parse error.
# expect+2: Invalid time value "10000000000000000000000000000000"
# expect+1: Malformed conditional (${:L:gmtime=10000000000000000000000000000000} != "")
.if ${:L:gmtime=10000000000000000000000000000000} != ""
.  error
.else
.  error
.endif

# Before var.c 1.631 from 2020-10-31, there was no error handling while
# parsing the :gmtime modifier, thus no error message was printed.  Parsing
# stopped after the '=', and the remaining string was parsed for more variable
# modifiers.  Because of the unknown modifier 'e' from the 'error', the whole
# variable value was discarded and thus not printed.
# expect+2: Invalid time value "error"
# expect+1: Malformed conditional (${:L:gmtime=error} != "")
.if ${:L:gmtime=error} != ""
.  error
.else
.  error
.endif

# Before var.c 1.1050 from 2023-05-09, the timestamp could be directly
# followed by the next modifier, without a ':' separator.  This was the same
# bug as for the ':L' and ':P' modifiers.
# expect+2: Invalid time value "100000S,1970,bad,"
# expect+1: Malformed conditional (${%Y:L:gmtime=100000S,1970,bad,} != "bad")
.if ${%Y:L:gmtime=100000S,1970,bad,} != "bad"
.  error
.endif


# Before var.c 1.1062 from 2023-08-19, ':gmtime' but not ':localtime' reported
# wrong values for '%s', depending on the operating system and the timezone.
export TZ=UTC
.for t in ${%s:L:gmtime} ${%s:L:localtime}
TIMESTAMPS+= $t
.endfor
export TZ=Europe/Berlin
.for t in ${%s:L:gmtime} ${%s:L:localtime}
TIMESTAMPS+= $t
.endfor
export TZ=UTC
.for t in ${%s:L:gmtime} ${%s:L:localtime}
TIMESTAMPS+= $t
.endfor
export TZ=America/Los_Angeles
.for t in ${%s:L:gmtime} ${%s:L:localtime}
TIMESTAMPS+= $t
.endfor
export TZ=UTC
.for t in ${%s:L:gmtime} ${%s:L:localtime}
TIMESTAMPS+= $t
.endfor
.for a b in ${TIMESTAMPS:[1]} ${TIMESTAMPS:@t@$t $t@} ${TIMESTAMPS:[-1]}
.  if $a > $b
.    warning timestamp $a > $b
.  endif
.endfor


.if ${year=%Y month=%m day=%d:L:gmtime=1459494000} != "year=2016 month=04 day=01"
.  error
.endif
# Slightly contorted syntax to convert a UTC timestamp from an expression to a
# formatted timestamp.
.if ${%Y%m%d:L:${gmtime=${:U1459494000}:L}} != "20160401"
.  error
.endif


all:
