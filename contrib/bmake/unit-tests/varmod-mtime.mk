# $NetBSD: varmod-mtime.mk,v 1.1 2023/05/09 20:14:27 sjg Exp $
#
# Tests for the :mtime variable modifier, which provides mtime
# of variable value assumed to be a pathname.

all:

# mtime of this makefile
mtime:= ${MAKEFILE:mtime}

# if pathname does not exist and timestamp is provided
# that is the result
.if ${no/such:L:mtime=0} != "0"
.  error
.endif

.if ${no/such:L:mtime=42} != "42"
.  error
.endif

# if no timestamp is provided and stat(2) fails use current time
.if ${no/such:L:mtime} < ${mtime}
.   error no/such:L:mtime ${no/such:L:mtime} < ${mtime}
.endif

COOKIE = ${TMPDIR}/varmod-mtime.cookie
x!= touch ${COOKIE}
.if ${COOKIE:mtime=0} < ${mtime}
.   error COOKIE:mtime=0 ${COOKIE:mtime=0} < ${mtime}
.endif
