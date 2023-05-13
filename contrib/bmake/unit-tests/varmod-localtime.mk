# $NetBSD: varmod-localtime.mk,v 1.12 2023/05/10 15:53:32 rillig Exp $
#
# Tests for the :localtime variable modifier, which formats a timestamp
# using strftime(3) in local time.
#
# See also:
#	varmod-gmtime.mk

.if ${TZ:Uno:NEurope/Berlin:NUTC-1} != ""	# see unit-tests/Makefile
.  error
.endif

# Test for the default time format, %c.  Since the time always varies, it's
# only possible to check for the general format here.  The names of the
# month and weekday are always in English, independent from the locale.
# Example: Thu Oct 29 18:56:41 2020
.if ${:U:localtime:tW:M??? ??? ?? ??\:??\:?? ????} == ""
.  error
.endif


# modifier name too short, falling back to the SysV modifier.
.if ${%Y:L:localtim=1593536400} != "%Y"
.  error
.endif


# 2020-07-01T00:00:00Z
.if ${%Y:L:localtime=1593536400} != "2020"
.  error
.endif


# modifier name too long, falling back to the SysV modifier.
.if ${%Y:L:localtimer=1593536400} != "%Y"
.  error
.endif


# If the modifier name is not matched exactly, fall back to the
# :from=to modifier.
.if ${localtime:L:local%=gm%} != "gmtime"
.  error
.endif


# Before var.c 1.1050 from 2023-05-09, it was not possible to pass the
# seconds via a variable expression.
.if ${%Y:L:localtime=${:U1593536400}} != "2020"
.  error
.endif


# Before var.c 1.631 from 2020-10-31 21:40:20, it was possible to pass
# negative time stamps to the :localtime modifier, resulting in dates before
# 1970.  Going back 50 years in the past is not a practical use case for
# make.  Therefore, since var.c 1.631, negative time stamps produce a
# parse error.
.if ${:L:localtime=-1} != ""
.  error
.else
.  error
.endif


# Spaces were allowed before var.c 1.631 from 2020-10-31 21:40:20, not
# because it would make sense but just as a side-effect from using strtoul.
.if ${:L:localtime= 1} != ""
.  error
.else
.  error
.endif


# 0 means now; this differs from GNode.mtime, where a 0 means nonexistent.
# Since "now" constantly changes, the strongest possible test is to match the
# resulting pattern.
.if !${:L:localtime=0:tW:M??? ??? ?? ??\:??\:?? 20??}
.  error
.endif


.if ${:L:localtime=1} != "Thu Jan  1 01:00:01 1970"
.  error
.endif


# INT32_MAX
.if ${:L:localtime=2147483647} != "Tue Jan 19 04:14:07 2038"
.  error
.endif


.if ${:L:localtime=2147483648} == "Tue Jan 19 04:14:08 2038"
# All systems that have unsigned time_t or 64-bit time_t.
.elif ${:L:localtime=2147483648} == "Fri Dec 13 21:45:52 1901"
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
.if ${:L:localtime=10000000000000000000000000000000} != ""
.  error
.else
.  error
.endif

# Before var.c 1.631 from 2020-10-31, there was no error handling while
# parsing the :localtime modifier, thus no error message was printed.  Parsing
# stopped after the '=', and the remaining string was parsed for more variable
# modifiers.  Because of the unknown modifier 'e' from the 'error', the whole
# variable value was discarded and thus not printed.
.if ${:L:localtime=error} != ""
.  error
.else
.  error
.endif

# Before var.c 1.1050 from 2023-05-09, the timestamp could be directly
# followed by the next modifier, without a ':' separator.  This was the same
# bug as for the ':L' and ':P' modifiers.
.if ${%Y:L:localtime=100000S,1970,bad,} != "bad"
.  error
.endif

all:
