# $NetBSD: varmod-mtime.mk,v 1.17 2025/06/28 22:39:29 rillig Exp $
#
# Tests for the ':mtime' variable modifier, which maps each word of the
# expression to that file's modification time.

# Note: strftime() uses mktime() for %s and mktime() assumes localtime
# so this should match time()
start:=	${%s:L:localtime}	# see varmod-gmtime.mk, keyword '%s'


# Ensure that this makefile exists and has a modification time.  If the file
# didn't exist, the ':mtime' modifier would return the current time.
.if ${MAKEFILE:mtime} >= ${start}
.  error
.endif


# For a file that doesn't exist, the ':mtime' modifier returns the current
# time, without an error or warning message.  The returned timestamp differs
# from the 'now' that is used when updating the timestamps in archives or for
# touching files using the '-t' option, which is taken once when make is
# started.
not_found_mtime:=	${no/such/file:L:mtime}
.if ${not_found_mtime} < ${start}
.  error
.endif


# The ':mtime' modifier accepts a timestamp in seconds as an optional
# argument.  This timestamp is used as a fallback in case the file's time
# cannot be determined, without any error or warning message.
.if ${no/such/file:L:mtime=0} != "0"
.  error
.endif


# The fallback timestamp must start with a digit, and it is interpreted as a
# decimal integer.
.if ${no/such/file:L:mtime=00042} != "42"
.  error
.endif


# The fallback timestamp must only be an integer, without trailing characters.
# expect+1: Invalid argument "123x" for modifier ":mtime"
.if ${no/such/file:L:mtime=123x}
.  error
.else
.  error
.endif


# The timestamp of a newly created file must be at least as great as the
# timestamp when parsing of this makefile started.
COOKIE=	${TMPDIR:U/tmp}/varmod-mtime.cookie
_!=	touch ${COOKIE}
.if ${COOKIE:mtime=0} < ${start}
.   error ${COOKIE:mtime=0} < ${start}
.endif
_!=	rm -f ${COOKIE}


# If the optional argument of the ':mtime' modifier is the word 'error', the
# modifier fails with an error message, once for each affected file.
#
# expect+2: Cannot determine mtime for "no/such/file1": <ENOENT>
# expect+1: Cannot determine mtime for "no/such/file2": <ENOENT>
.if ${no/such/file1 no/such/file2:L:mtime=error}
.  error
.else
.  error
.endif


# Only the word 'error' is a special argument to the ':mtime' modifier, all
# other words result in a parse error.
# expect+1: Invalid argument "errorhandler-no" for modifier ":mtime"
.if ${MAKEFILE:mtime=errorhandler-no} > 0
.else
.  error
.endif


# Only the word 'error' can be used as a fallback argument to the modifier.
# expect+1: Invalid argument "warn" for modifier ":mtime"
.if ${MAKEFILE:mtime=warn} > 0
.  error
.else
.  error
.endif


# Ensure that the fallback for a missing modification time is indeed the
# current time, and not any later time.
end:=	${%s:L:gmtime}
.if ${not_found_mtime} > ${end}
.  error
.endif


# If the expression is irrelevant, the ':mtime' modifier is only parsed, it
# does not perform any filesystem operations.
.if 0 && ${no/such/file:L:mtime=error}
.  error
.endif


# If there is a typo in the modifier name, it does not match.
# expect+1: Unknown modifier ":mtim"
.if ${anything:L:mtim}
.  error
.else
.  error
.endif


# An empty word list results in an empty mtime list.
.if ${:U:mtime} != ""
.  error
.endif
